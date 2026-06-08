.. _mathematical_models:

Mathematical Models
===================

This page documents the mathematical models, physical parameterizations, and numerical formulations available in ``massconsistent_amr``. Each section is documented with references to the scientific literature supporting its implementation. For full citations, see the :ref:`references` page.

.. contents:: Topics
   :local:
   :depth: 2

Core Mass-Consistent Wind Solver
--------------------------------

The primary solver implements the variational, mass-consistent wind field adjustment methodology based on Sherman (1978) and Mathiesen (1987). This diagnostic model adjusts an initial wind profile over complex terrain to enforce mass conservation while minimizing alterations to the initial flow field. The practical implementation follows the QUIC-URB architecture (Pardyjak & Brown, 2001).

Terrain Interpolation
~~~~~~~~~~~~~~~~~~~~~
An arbitrary-density terrain point cloud (X, Y, Z) is ingested from a CSV file. The terrain elevation :math:`z_{\text{terrain}}(i,j)` at each grid column center is interpolated from the six nearest data points using inverse-distance weighting (IDW):

.. math::

   z_{\text{terrain}}(x,y) = \frac{\sum_{n=1}^6 w_n z_n}{\sum_{n=1}^6 w_n}, \quad w_n = \frac{1}{( (x - x_n)^2 + (y - y_n)^2 )^2}

Wind Profile Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~
The initial velocity components :math:`(u_0, v_0, w_0)` are computed for every cell. Above the local terrain surface, the default **log-law** wind profile is constructed at height above ground level (AGL), following Monin-Obukhov similarity theory (Monin & Obukhov, 1954; Stull, 1988):

.. math::

   z_{\text{agl}}(i,j,k) = z_{\text{physical}}(k) - z_{\text{terrain}}(i,j)

.. math::

   u_0(z_{\text{agl}}) = \frac{u_*}{\kappa}\,\ln\!\left(\frac{z_{\text{agl}}+z_0}{z_0}\right) \hat{u}_x, \quad
   v_0(z_{\text{agl}}) = \frac{u_*}{\kappa}\,\ln\!\left(\frac{z_{\text{agl}}+z_0}{z_0}\right) \hat{u}_y, \quad
   w_0(z_{\text{agl}}) = 0

where :math:`\kappa = 0.41` is the von Kármán constant (von Kármán, 1948), :math:`z_0` is the aerodynamic roughness length, and the friction velocity :math:`u_*` is obtained from the reference wind speed :math:`|\mathbf{U}_{\text{ref}}|` at reference height :math:`z_{\text{ref}}`:

.. math::

   u_* = \frac{\kappa\,|\mathbf{U}_{\text{ref}}|}{\ln\!\left(\dfrac{z_{\text{ref}}+z_0}{z_0}\right)}

Topographic Barrier Shielding
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
In complex terrain, observations from meteorological stations located in different valleys can unphysically distort the initial interpolated wind field if they are separated by high ridges. To address this, the solver supports CALMET-style topographic barrier shielding.

When ``enable_topographic_shielding = true`` is configured, the interpolation weight between a query grid cell :math:`(x_q, y_q, z_q)` and a station :math:`(x_i, y_i, z_i)` is heavily penalized (by a factor of :math:`10^{-5}`) if the direct line-of-sight segment connecting them intersects the terrain height field :math:`z_{\text{terrain}}(x, y)`.

Specifically, along the line-of-sight parametrized by :math:`t \in [0, 1]`:

.. math::

   x(t) = x_q + t(x_i - x_q), \quad y(t) = y_q + t(y_i - y_q), \quad z(t) = z_q + t(z_i - z_q)

The blocking condition is checked at discrete intervals:

.. math::

   z_{\text{terrain}}(x(t), y(t)) > z(t)

If blocking is detected, the weight :math:`w_i` for that station is reduced:

.. math::

   w_i \leftarrow w_i \times 10^{-5}

If all nearest stations are blocked for a given query cell, the solver dynamically falls back to unpenalized weights to avoid numerical division-by-zero and preserve signal continuity.

Variational Formulation
~~~~~~~~~~~~~~~~~~~~~~~
The corrected wind field :math:`\mathbf{u} = (u,v,w)` is obtained by minimizing the volume integral of the difference between the adjusted and initial velocity fields, weighted by directional penalty coefficients (Sherman, 1978; Mathiesen, 1987):

.. math::

   E(u,v,w,\lambda) = \int\!\left[\frac{(u-u_0)^2}{\alpha_h^2} + \frac{(v-v_0)^2}{\alpha_h^2} + \frac{(w-w_0)^2}{\alpha_v^2} + \lambda \left(\frac{\partial u}{\partial x} + \frac{\partial v}{\partial y} + \frac{\partial w}{\partial z}\right)\right]\mathrm{d}V

where :math:`\lambda` is a Lagrange multiplier field, and :math:`\alpha_h`, :math:`\alpha_v` are horizontal and vertical anisotropy coefficients. Taking the variation with respect to :math:`u, v, w` and setting it to zero yields the adjusted velocities:

.. math::

   u = u_0 - \alpha_h^2 \frac{\partial\lambda}{\partial x}, \quad
   v = v_0 - \alpha_h^2 \frac{\partial\lambda}{\partial y}, \quad
   w = w_0 - \alpha_v^2 \frac{\partial\lambda}{\partial z}

Taking the variation with respect to :math:`\lambda` yields the mass-conservation constraint:

.. math::

   \frac{\partial u}{\partial x} + \frac{\partial v}{\partial y} + \frac{\partial w}{\partial z} = 0

Substituting the adjusted velocities into the divergence equation results in the anisotropic Poisson equation for :math:`\lambda`:

.. math::

   -\left(\alpha_h^2\frac{\partial^2\lambda}{\partial x^2} + \alpha_h^2\frac{\partial^2\lambda}{\partial y^2} + \alpha_v^2\frac{\partial^2\lambda}{\partial z^2}\right) = -\nabla\cdot\mathbf{u}_0

Boundary Conditions
~~~~~~~~~~~~~~~~~~~
The anisotropic Poisson equation is solved using the following boundary conditions:
* **Inflow/Outflow (x-faces)**: Dirichlet :math:`\lambda = 0` (preserves the boundary velocity)
* **Lateral (y-faces)**: Neumann :math:`\frac{\partial\lambda}{\partial y} = 0` (no flow adjustment normal to lateral faces)
* **Ground and Top (z-faces)**: Neumann :math:`\frac{\partial\lambda}{\partial z} = 0` (no flow adjustment normal to ground/top)

Advanced Boundary Layer Physics
-------------------------------

To represent complex microscale atmospheric dynamics, several physical models are integrated. These follow established boundary layer meteorology theory (Stull, 1988; Högström, 1996).

Atmospheric Stability (Monin-Obukhov Similarity Theory)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The log-law wind profile can be adjusted for non-neutral thermal stratification using stability corrections (Businger et al., 1971; Dyer, 1974). The corrected profile follows:

.. math::

   u(z) = \frac{u_*}{\kappa}\left[\ln\left(\frac{z_{\text{agl}}+z_0}{z_0}\right) - \psi_m\left(\frac{z_{\text{agl}}}{L}\right) + \psi_m\left(\frac{z_0}{L}\right)\right]

where :math:`L` is the Obukhov length characterizing atmospheric stability (positive for stable, negative for unstable) (Monin & Obukhov, 1954).

For **stable conditions** (:math:`\zeta = z_{\text{agl}}/L > 0`), the Holtslag-De Bruin formulation is used (Holtslag & De Bruin, 1988):

.. math::

   \psi_m(\zeta) = -5\zeta

For **unstable conditions** (:math:`\zeta < 0`):

.. math::

   \psi_m(\zeta) = 2\ln\left(\frac{1+x}{2}\right) + \ln\left(\frac{1+x^2}{2}\right) - 2\arctan(x) + \frac{\pi}{2}

where :math:`x = (1 - 16\zeta)^{1/4}`.

Bulk Richardson Stability Model Selection
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The solver automatically selects between Businger-Dyer and Holtslag-De Bruin stability models based on the bulk Richardson number :math:`Ri_b`:

.. math::

   Ri_b = \frac{g}{\theta_{\text{ref}}} \frac{\Delta\theta \cdot h}{U^2}

where :math:`\Delta\theta` is the potential temperature difference, and :math:`h` is height above ground level.
* **Weak Stability (:math:`Ri_b < 0.1`)**: Uses Businger-Dyer functions.
* **Strong Stability (:math:`Ri_b \ge 0.1`)**: Uses Holtslag-De Bruin functions to represent strong vertical shear damping.

Spatially-Varying Diagnostic Boundary Layer Height
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To determine the mixing depth and boundary layer height :math:`z_i(x,y)` dynamically over complex terrain and variable thermal conditions, the solver implements a column-scanning bulk Richardson number (:math:`Ri_b`) profile method when ``enable_bl_depth_diagnostic = true``.

The diagnostic scans each vertical grid column from the first cell above ground level, :math:`k_{\text{start}}`, up to the top of the domain. At each level :math:`k`, the bulk Richardson number is calculated using the local height above ground level :math:`z_{\text{agl}}` and the potential temperature difference relative to the surface cell:

.. math::

   Ri_b(z_{\text{agl}}) = \frac{g}{\theta_s} \frac{[\theta(z_{\text{agl}}) - \theta_s] \cdot z_{\text{agl}}}{u^2(z_{\text{agl}}) + v^2(z_{\text{agl}})}

where :math:`\theta_s` is the surface potential temperature at :math:`k_{\text{start}}`, and :math:`u(z_{\text{agl}}), v(z_{\text{agl}})` are horizontal wind components.

The boundary layer depth :math:`z_i(x,y)` is diagnosed as the height above ground where :math:`Ri_b` first exceeds the critical Richardson number :math:`Ri_c` (configured via ``richardson_critical``, defaulting to 0.25). Linear interpolation between grid levels is used to compute the precise transition height. If :math:`Ri_b` never exceeds :math:`Ri_c`, the boundary layer is assumed to extend to the top of the domain.

Jackson-Hunt Orographic Speed-up Model
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Wind acceleration over convex terrain features (ridges, hill tops) and deceleration in valleys is parameterized using the Jackson and Hunt (1975) model, which has been validated experimentally over low hills (Ayotte et al., 1994; Belcher et al., 1994):

.. math::

   \Delta U_{\text{speedup}}(z) = U_0(z) \cdot \left[ a \cdot \kappa_s \cdot L \cdot \left( \frac{z}{L} \right) \exp\left(-\frac{z}{L}\right) \right]

where :math:`\kappa_s` is local terrain curvature (positive for ridges, negative for valleys), :math:`L` is the half-width of the dominant terrain feature, and :math:`a` is a tuning coefficient.

The model is activated based on a **Froude number and slope threshold**:
1. **Froude number constraint**: :math:`Fr = \frac{U}{N \cdot H} > 0.1` (where :math:`N` is Brunt-Väisälä frequency, :math:`H` is obstacle height).
2. **Slope constraint**: local slope magnitude :math:`|\nabla h| > 0.05`.

Thermally-Driven Slope Flows (Katabatic & Anabatic)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Thermally-driven up-slope (anabatic, daytime) and down-slope (katabatic, nighttime) flows are parameterized as:

.. math::

   u_{\text{slope}}(z) = U_{\text{max}} \cdot \sin(\theta_{\text{slope}}) \cdot \left( \frac{z_{\text{agl}}}{z_{\text{max}}} \right) \exp\left( 1 - \frac{z_{\text{agl}}}{z_{\text{max}}} \right)

where :math:`\theta_{\text{slope}}` is the local terrain slope angle, :math:`z_{\text{max}}` is the height of maximum slope flow velocity, and :math:`U_{\text{max}}` scales with surface sensible heat flux.

Sea Breeze Circulation
~~~~~~~~~~~~~~~~~~~~~~
A sea breeze thermal circulation is modeled near coastlines driven by land-sea temperature contrast:

.. math::

   u_{\text{sea\_breeze}}(x,z) = U_{\text{sb\_max}} \cdot \sin\left(\frac{\pi x}{L_{\text{coast}}}\right) \cdot \frac{z_{\text{agl}}}{z_{\text{sb}}} \exp\left(1 - \frac{z_{\text{agl}}}{z_{\text{sb}}}\right)

where :math:`L_{\text{coast}}` is the thermal influence scale, and :math:`z_{\text{sb}}` is the circulation height.

Froude Number Terrain Blocking
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Under highly stable conditions, flow blocking and lateral channeling around steep mountains is parameterized when the Froude number :math:`Fr < 1`:

.. math::

   u_{\text{blocked}} = u_0 \cdot \left[1 - (1 - Fr) \cdot \exp\left(-\frac{z_{\text{agl}}}{H_c}\right)\right]

where :math:`H_c` is the critical dividing stream height.

Canopy and Obstacle Modeling
----------------------------

MacDonald Forest Canopy Drag Model
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Spatiotemporally varying vegetative canopies are parameterized using MacDonald et al. (2000) and Shaw-Pereira (1982) formulations. The exponential canopy velocity decay and displacement height calculations follow established canopy aerodynamic theory (Raupach, 1994; Nakai et al., 2012). Within the canopy height (:math:`z_{\text{agl}} \le h_c`), the wind velocity decays exponentially:

.. math::

   u(z_{\text{agl}}) = u(h_c) \cdot \exp\left[ -\alpha_{\text{canopy}} \left( 1 - \frac{z_{\text{agl}}}{h_c} \right) \right]

where :math:`\alpha_{\text{canopy}}` is the exponential attenuation coefficient (typically 2.0 to 4.0).
Above the canopy height (:math:`z_{\text{agl}} > h_c`), the standard log-law profile is modified with a displacement height :math:`d`:

.. math::

   u(z_{\text{agl}}) = \frac{u_*}{\kappa} \ln\left( \frac{z_{\text{agl}} - d + z_0}{z_0} \right)

where :math:`d` and effective :math:`z_0` are computed based on canopy plan area index :math:`\lambda_p` and frontal area index :math:`\lambda_f`.

Building Wake Modeling (Röckle, Huber-Snyder, AERMOD PRIME)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Solid structures read from a buildings CSV file are masked (zero velocity inside). Their aerodynamic wakes are modeled using three selectable parameterizations applied to the initial wind field. These implementations follow regulatory and wind engineering standards:

1. **Röckle (1990) Model** — Empirical cavity and far-wake parameterization for urban flows (Röckle, 1990).
2. **Huber-Snyder (EPA) Model** — Power-law wake deficit formulation from wind engineering (Huber & Snyder, 1982; Snyder, 1981).
3. **AERMOD PRIME (EPA) Model** — Regulatory model for building downwash (Cimorelli et al., 2005; EPA 2005).
   * Cavity length :math:`L_r = c_1 \cdot H` (where :math:`H` is building height).
   * Cavity velocity deficit: :math:`u_{\text{deficit}} = c_2 \cdot U_H` with vertical rooftop vortex circulation patterns.
   * Far-wake velocity deficit decays linearly to zero at distance :math:`L_f = 3H`.

2. **Huber-Snyder (EPA) Model**:
   * Cavity length scales with building aspect ratio: :math:`L_c = 0.5 \cdot H \cdot \sqrt{W/H}`.
   * Far-wake velocity deficit uses a power-law decay: :math:`u_{\text{deficit}} \propto \frac{1}{\sqrt{x/L_c}}` extending to :math:`5H`.

3. **AERMOD PRIME (EPA) Model**:
   * Uses Projected Building Area (PBA) perpendicular to wind direction to compute effective building dimensions.
   * Far-wake velocity deficit decays exponentially: :math:`u_{\text{deficit}} \propto \exp\left(-1.5 \frac{x-L_c}{10H - L_c}\right)`.

**Adaptive Wake Superposition and Blending**:
Instead of exclusive zone assignments, overlapping wakes from multiple buildings are smoothly blended using distance-weighted exponential functions:

.. math::

   w_i = \exp\left(-\frac{d_i}{L_{\text{blend}}}\right)

where :math:`d_i` is distance to building :math:`i`'s wake boundary, and :math:`L_{\text{blend}} \approx 0.5 H`.

Building Street Canyon Vortex Parameterization (QUIC-URB Style)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
When parallel buildings are aligned perpendicular to the ambient wind direction, the solver identifies street canyons geometrically, computes their aspect ratio (:math:`H/W`), and overwrites the initial wind field inside the canyon with a parameterized, solenoidal (divergence-free) recirculating vortex profile before the Poisson solve (Pardyjak & Brown, 2001; Brown et al., 2000):

* **Solenoidal Vortex Velocity Components**:
  
  .. math::

     u_{\text{vortex}}(x, z) = -C_{\text{vortex}} \cdot U_{\text{ambient}} \cdot \cos\left(\pi \frac{z}{H}\right) \cdot \sin\left(\pi \frac{x - x_{\text{up}}}{W}\right)

  .. math::

     w_{\text{vortex}}(x, z) = C_{\text{vortex}} \cdot U_{\text{ambient}} \cdot \left(\frac{H}{W}\right) \cdot \sin\left(\pi \frac{z}{H}\right) \cdot \cos\left(\pi \frac{x - x_{\text{up}}}{W}\right)

* **Regime-Dependent Vortex Strength**:
  * For skimming flow (:math:`H/W > 0.7`): :math:`C_{\text{vortex}} = 0.25` (full recirculation).
  * For wake interference flow (:math:`0.3 < H/W \le 0.7`): :math:`C_{\text{vortex}}` scales linearly from 0 to 0.25.
  * For isolated roughness flow (:math:`H/W \le 0.3`): :math:`C_{\text{vortex}} = 0.0`.

Analytical Wind Turbine Wake Models
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The solver supports analytical turbine wake deficits for wind energy applications, following established models in wind farm design and optimization:

1. **Jensen (Park) Model** (Jensen, 1983; Katic et al., 1986) — Classic linear wake expansion model widely used in wind farm calculations.
   
   .. math::
   
      R_w(x_{\text{down}}) = R_0 + k_w \cdot x_{\text{down}}

   .. math::
   
      \Delta U = U_0 \cdot \frac{1 - \sqrt{1 - C_T}}{(1 + 2 k_w x_{\text{down}} / D)^2}

2. **Bastankhah (Gaussian) Model** (Bastankhah & Porté-Agel, 2014) — Gaussian wake deficit based on top-hat distribution, widely used in wind energy applications.
   
   .. math::
   
      \sigma_w(x_{\text{down}}) = k_a \cdot x_{\text{down}} + \epsilon \cdot D
 
   .. math::
   
      \frac{\Delta U}{U_0} = \left( 1 - \sqrt{1 - \frac{C_T}{8 (\sigma_w / D)^2}} \right) \cdot \exp\left( - \frac{r^2}{2 \sigma_w^2} \right)

3. **TurbOPark Model** — A self-similar Gaussian deficit model that uses a wake expansion parameter based on local turbulence intensity, implemented from wind energy research (Crespo et al., 1999; Frandsen et al., 2006):
   
   .. math::
   
      \sigma_w(x_{\text{down}}) = \sigma_0 + k_w \cdot x_{\text{down}}
      
   where :math:`\sigma_0 = 0.25 D` is the initial wake width at the rotor disk and the wake expansion rate :math:`k_w` is parameterized by:

   .. math::

      k_w = c_1 \cdot TI_{\text{local}}

   with empirical scaling coefficient :math:`c_1 \approx 0.38`.

4. **Gauss-Curl Hybrid (GCH) Model** (Martínez-Tossas & Meneveau, 2019; Qian & Ishihara, 2016; Howland et al., 2016) — An advanced model that resolves secondary steering effects (including counter-rotating vortex pairs) generated by yawed turbines.
   
   The spanwise and vertical vortices are resolved in a right-handed Cartesian coordinate system with:

   * :math:`x_{\text{down}}` positive in the downstream/streamwise direction.
   * :math:`y` positive in the spanwise direction (extending to the observer's left when looking downstream, equivalent to the port side of the turbine).
   * :math:`z` positive vertically upwards.

   The initial strength of the counter-rotating vortex pair :math:`\Gamma_0` is computed as:

   .. math::

      \Gamma_0 = \frac{1}{2} C_T D U_{\infty} \cos^2(\gamma) \sin(\gamma)

   and the vortex circulation strength decays exponentially downstream according to:

   .. math::

      \Gamma(x_{\text{down}}) = \Gamma_0 \exp\left( -c_{\text{decay}} \frac{x_{\text{down}}}{D} \right)

   where :math:`c_{\text{decay}} = 0.1` is the empirical decay scaling coefficient. The spanwise :math:`v_{\text{vortex}}` and vertical :math:`w_{\text{vortex}}` velocity perturbations induced by the vortex cores at :math:`(y_c, z_c) = (\pm 0.5D, \pm 0.25D)` are calculated as:

   .. math::

      v_{\text{vortex}}(y,z) = \frac{\Gamma(x_{\text{down}})}{2\pi} \left[ \frac{z - z_c}{(y - y_c)^2 + (z - z_c)^2} - \frac{z + z_c}{(y - y_c)^2 + (z + z_c)^2} \right]

   .. math::

      w_{\text{vortex}}(y,z) = -\frac{\Gamma(x_{\text{down}})}{2\pi} \left[ \frac{y - y_c}{(y - y_c)^2 + (z - z_c)^2} - \frac{y + y_c}{(y - y_c)^2 + (z + z_c)^2} \right]

   These induced crosswind velocities steer downstream turbine wakes directly within the 3D velocity grid, fully capturing multi-turbine secondary steering without the computational expense of full CFD.

Wake Deficit Superposition Methods
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To combine velocity deficits from multiple overlapping upstream turbine wakes at a grid point, the solver supports three superposition options:

1. **Quadratic Superposition (RSS)** (default):
   The combined deficit :math:`\Delta u_{\text{comb}}` is the root-sum-squares of the individual deficits:

   .. math::

      \Delta u_{\text{comb}} = \sqrt{\sum_{i} \Delta u_i^2}

2. **Linear Superposition**:
   The combined deficit is the linear sum of individual deficits (applied as a direct product of speed reduction factors):

   .. math::

      \Delta u_{\text{comb}} = \sum_{i} \Delta u_i

3. **Maximum Deficit Superposition (MAX)**:
   The combined deficit is the maximum of the individual deficits:

   .. math::

      \Delta u_{\text{comb}} = \max_i(\Delta u_i)

Wake Centerline Deflection Models
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. **Jimenez Model** (Jimenez, 2010):
    To model yawed wind turbine wakes, the Jimenez wake deflection model computes the wake centerline deflection :math:`y_{\text{offset}}(x_{\text{down}})` due to thrust-induced lateral force components:

.. math::

   \theta_0 = \frac{C_T}{2} \cos^2(\gamma) \sin(\gamma)

The deflection angle :math:`\theta` decays with downstream distance as:

.. math::

   \theta(x_{\text{down}}) = \frac{\theta_0}{\left(1 + 2 \beta_{\text{def}} \frac{x_{\text{down}}}{D}\right)^2}

Integrating the deflection angle along the downstream path yields the transverse wake offset:

.. math::

   y_{\text{offset}}(x_{\text{down}}) = D \cdot \theta_0 \cdot \frac{1}{2 \beta_{\text{def}}} \left( 1 - \frac{1}{1 + 2 \beta_{\text{def}} \frac{x_{\text{down}}}{D}} \right)

where :math:`\gamma` is the yaw angle and :math:`\beta_{\text{def}} = k_d` is the deflection decay coefficient (configured via `jimenez_kd`, defaulting to 0.05).

2. **Bastankhah & Porté-Agel Model (2016)** (Bastankhah & Porté-Agel, 2016):
    The Bastankhah & Porté-Agel wake deflection model is a closed-form mass-and-momentum-conserving analytical formulation for Gaussian wakes in yawed conditions. The initial skew angle at the rotor is given by:

.. math::

   \theta_{c0} = \frac{0.3 \gamma}{\cos \gamma} \left(1 - \sqrt{1 - C_T \cos \gamma}\right)

The deflection :math:`\delta(x_{\text{down}})` is computed separately for near and far wake regions bounded by the near-wake length :math:`x_0`:

* For :math:`x_{\text{down}} \le x_0` (near-wake):

  .. math::

     \delta(x_{\text{down}}) = \tan(\theta_{c0}) \cdot x_{\text{down}}

* For :math:`x_{\text{down}} > x_0` (far-wake):

  .. math::

     \delta(x_{\text{down}}) = \tan(\theta_{c0}) \cdot x_0 + \theta_{c0} \frac{E_0}{5.2} \sqrt{\frac{\sigma_{y0} \sigma_{z0}}{k^2 M_0}} \ln \left[ \frac{(1.6 + \sqrt{M_0})(1.6 \sqrt{\frac{\sigma_y \sigma_z}{\sigma_{y0} \sigma_{z0}}} - \sqrt{M_0})}{(1.6 - \sqrt{M_0})(1.6 \sqrt{\frac{\sigma_y \sigma_z}{\sigma_{y0} \sigma_{z0}}} + \sqrt{M_0})} \right]

where :math:`M_0 = C_0(2 - C_0)`, :math:`C_0 = 1 - \sqrt{1 - C_T}`, and :math:`E_0 = C_0^2 - 3 e^{1/12} C_0 + 3 e^{1/3}`.

Vertical Wake Deflection (Tilt Model)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Rotor tilt angle :math:`\theta_{\text{tilt}}` (tilting the rotor disk backwards or upwards) is supported to calculate basic vertical wake deflection, which is particularly relevant for floating offshore wind turbines. The vertical wake deflection :math:`z_{\text{offset}}(x_{\text{down}})` is computed using a vertical formulation analogous to the Jimenez deflection model:

.. math::

   \theta_{v0} = \frac{C_T}{2} \cos^2(\theta_{\text{tilt}}) \sin(\theta_{\text{tilt}})

The resulting vertical deflection offset at downstream distance :math:`x_{\text{down}}` is:

.. math::

   z_{\text{offset}}(x_{\text{down}}) = D \cdot \theta_{v0} \cdot \frac{1}{2 \beta_{\text{def}}} \left( 1 - \frac{1}{1 + 2 \beta_{\text{def}} \frac{x_{\text{down}}}{D}} \right)

Height-Varying (Veered) Wake Orientation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Under veered atmospheric conditions, the wind direction changes continuously with height. The coordinate projection used to define "downwind" and "crosswind" directions is modified dynamically to use the local wind direction at each vertical grid level :math:`z` rather than strictly the wind direction at hub height. This is a pure algebraic coordinate transformation that captures wake twisting under atmospheric wind veer.

Analytical Wake-Added Turbulence Models
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Turbines increase local downstream turbulence intensity :math:`TI_{\text{local}} = \sqrt{TI_{\text{ambient}}^2 + \sum \Delta I_{+}^2}` via established empirical models (Crespo et al., 1999; Crespo & Hernández, 1996; Frandsen et al., 2006):

1. **Crespo-Hernández Model** (Crespo & Hernández, 1996):
   
   .. math::
   
      \Delta I_{+} = c_{\text{ch1}} \cdot a^{c_{\text{ch2}}} \cdot TI_{\text{ambient}}^{c_{\text{ch3}}} \cdot \left( \frac{x_{\text{down}}}{D} \right)^{c_{\text{ch4}}}

   where :math:`a = \frac{1 - \sqrt{1 - C_T}}{2}` is the axial induction factor, and the empirical scaling coefficients are:
   
   * :math:`c_{\text{ch1}} = 0.73`
   * :math:`c_{\text{ch2}} = 0.832`
   * :math:`c_{\text{ch3}} = 0.0325`
   * :math:`c_{\text{ch4}} = -0.32`

2. **Frandsen (STF) Model** (Frandsen et al., 2006):
   
   .. math::
   
      \Delta I_{+} = \frac{1}{c_{\text{fr1}} + c_{\text{fr2}} \cdot \frac{x_{\text{down}}}{D} / \sqrt{C_T}}

   where the empirical scaling coefficients are:

   * :math:`c_{\text{fr1}} = 1.5`
   * :math:`c_{\text{fr2}} = 0.8`

3. **Wake Recovery under Thermal Buoyancy (Buoyant Wake Destruction)** (Mirocha et al., 2018):
   In highly convective, unstable atmospheres, buoyancy-driven thermals rapidly break down wind turbine wakes. This buoyant destruction is parameterized by increasing the downstream decay rates of wake-added turbulence proportionally to the surface sensible heat flux :math:`H_s` (only when :math:`H_s > 0`):

   * For the **Crespo-Hernández** model, the decay exponent :math:`c_{\text{ch4}}` is modified as:

     .. math::

        c_{\text{ch4}} = -0.32 \cdot (1.0 + \beta_{\text{buoy}} \cdot H_s)

   * For the **Frandsen** model, the decay coefficient :math:`c_{\text{fr2}}` is modified as:

     .. math::

        c_{\text{fr2}} = 0.8 \cdot (1.0 + \beta_{\text{buoy}} \cdot H_s)

   where :math:`H_s` is the surface sensible heat flux in W/m² (parameter ``surface_sensible_heat_flux``), and :math:`\beta_{\text{buoy}}` is the buoyant wake destruction coefficient in m²/W (parameter ``buoyant_wake_destruction_coeff``, default 0.005).

The wake centerline conforms perfectly to local terrain height, bending over hills:

.. math::

   z_{\text{centerline}} = z_{\text{terrain}}(x,y) + H_{\text{hub}}

The wake expansion rates are scaled dynamically based on atmospheric stability factor :math:`F_{\text{stability}} \propto \tanh(H_{\text{hub}}/L)`.

Wake-Ground Interaction (Mirroring & Shear-Damping)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To account for ground boundary effects on expanding wakes, an analytical mirroring technique is used. A symmetric "mirror" turbine is placed below the local terrain surface at height :math:`-H_{\text{hub}}`. The total deficit is obtained by superposing the physical and mirrored wake deficits.

Additionally, to represent the high-shear surface layer damping when wakes overlap with the terrain, a shear-damping factor :math:`F_{\text{damp}}` is applied to both deficits:

.. math::

   F_{\text{damp}} = 1 - \exp\left( -\frac{z_{\text{agl}}}{d_{\text{scale}}} \right)

where :math:`z_{\text{agl}}` is the height above ground level and :math:`d_{\text{scale}} = 0.25 \cdot D` (configurable via `wake_ground_damping_scale`).

Annual Energy Production (AEP) Calculator
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The Python-side ``AEPCalculator`` computes the annual energy production of a wind farm over a multi-directional and multi-speed wind rose. Let :math:`N_{\theta}` be the number of wind direction sectors (e.g. 12 or 36 sectors, corresponding to :math:`\theta \in [0, 360)`), and :math:`N_U` be the number of wind speed bins.

The joint probability distribution of wind speed and direction is given by :math:`P(\theta_i, U_j)` such that:

.. math::

   \sum_{i=1}^{N_{\theta}} \sum_{j=1}^{N_U} P(\theta_i, U_j) = 1

For each wind state :math:`(\theta_i, U_j)`, the mass-consistent wind solver calculates the local inflow wind speed :math:`u_{\text{inflow}, t}(\theta_i, U_j)` at each wind turbine :math:`t \in \{1, \dots, N_T\}` taking into account terrain, masking, yaw, wake deficits, and secondary steering.

The power output :math:`P_{t}(u_{\text{inflow}, t})` of turbine :math:`t` is interpolated from its power curve. The total Annual Energy Production (in kWh) is the weighted sum of power outputs over all states, multiplied by the number of hours in a year (8760):

.. math::

   \text{AEP} = 8760 \times \sum_{i=1}^{N_{\theta}} \sum_{j=1}^{N_U} P(\theta_i, U_j) \left( \sum_{t=1}^{N_T} P_t\left(u_{\text{inflow}, t}(\theta_i, U_j)\right) \right)

where:

* :math:`P_t` is the turbine power in kW.
* :math:`8760` is the total number of hours in a non-leap year.
* Wind direction is defined using standard meteorological conventions. Here, :math:`\theta` represents the direction from which the wind blows (0° = North, 90° = East, 180° = South, 270° = West).
* The solver automatically rotates the inflow velocity vector to align with this meteorological convention before executing the mass-consistent Poisson solve on the Cartesian computational grid.

Gaussian Puff Dispersion Model
------------------------------

The passive Gaussian puff model couples wind transport with chemical/physical decay and deposition processes (Csanady, 1973; Seinfeld & Pandis, 2016):

Concentration Superposition
~~~~~~~~~~~~~~~~~~~~~~~~~~~
The concentration field is computed as a sum of discrete Gaussian-shaped puffs:

.. math::

    C(x,y,z,t) = \sum_i \frac{m_i}{(2\pi)^{3/2} \sigma_{x,i} \sigma_{y,i} \sigma_{z,i}} \exp\left(-\frac{(x-x_i)^2}{2\sigma_{x,i}^2} - \frac{(y-y_i)^2}{2\sigma_{y,i}^2} - \frac{(z-z_i)^2}{2\sigma_{z,i}^2}\right)

Puff Advection & Growth
~~~~~~~~~~~~~~~~~~~~~~~
Puff centers drift with the local wind velocity :math:`\mathbf{u}(\mathbf{r}_i)` and grow due to turbulent diffusion :math:`K`:

.. math::

   \mathbf{r}_i(t + \Delta t) = \mathbf{r}_i(t) + \mathbf{u}(\mathbf{r}_i) \Delta t, \quad
   \sigma(t + \Delta t) = \sqrt{\sigma^2(t) + 2 K \Delta t}

Height-Dependent Diffusivity :math:`K(z)`
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vertical eddy diffusivity :math:`K_v(z)` scales with height using power-law profiles:

.. math::

   K_v(z) = K_0 \cdot \left( \frac{z_{\text{agl}}}{z_{\text{ref}}} \right)^n

where :math:`n = 0.5` for neutral, :math:`1.2` for unstable, and :math:`0.3` for stable boundary layers.

Briggs Plume Rise
~~~~~~~~~~~~~~~~~
Buoyant exhaust plumes rise according to Briggs (1975) formula, which is the standard in environmental dispersion modeling (Briggs, 1975, 1984; Ooms et al., 1972):

.. math::

   \Delta h = \frac{1.6 F^{1/3} x^{2/3}}{u}, \quad F = \frac{g \cdot Q_H}{\rho \cdot c_p \cdot T}

where :math:`Q_H` is the thermal power release, and :math:`F` is the buoyancy flux.

Dry Deposition and Gravitational Settling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Mass removal due to ground deposition is computed when a puff overlaps the terrain surface (:math:`z_{\text{agl}} < 3 \sigma_z`). The deposition velocity is computed using Stokes' Law and particle settling theory (Slinn & Slinn, 1980; Slinn et al., 1978):

.. math::

   \Delta m = C_{\text{ground}} \cdot v_d \cdot A_{\text{eff}} \cdot \Delta t

where :math:`v_d` is the deposition/settling velocity, and :math:`A_{\text{eff}} \approx \pi (2 \sigma_y)^2` is the footprint area.

Ambient-Condition-Driven Chemical Decay
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Puff chemical molecular parameters and first-order exponential decay are coupled (Atkinson, 1994; Finlayson-Pitts & Pitts, 2000):

.. math::

   m_i(t) = m_i(0) \exp(-\lambda_{\text{decay}} t)

where :math:`\lambda_{\text{decay}}` is the first-order reaction/decay constant.

Synthetic Turbulence & Fluctuations
-----------------------------------

To synthesize terrain-aware turbulent fluctuations, the model uses spectral pipelines coupled with physical boundaries (Panofsky & Dutton, 1984; Veers, 1988).

Spectral Models
~~~~~~~~~~~~~~~
Turbulent velocity fluctuations can be synthesized using multiple advanced spectral models. These include standard isotropic/sheared models from wind engineering standards (IEC 61400-1, 2019; Sathe et al., 2011) and full anisotropic spectral tensor formulations (Mann Box).

IEC 61400-1 Spectral Models
^^^^^^^^^^^^^^^^^^^^^^^^^^^
The standard wind input models defined in IEC 61400-1:2019 (IEC, 2019) specify turbulence parameters for wind turbine design and certification, supporting the **Normal Turbulence Model (NTM)** and **Extreme Turbulence Model (ETM)**.

* **Turbine Power Classes**:
  
  - **Class I**: High wind sites, reference speed :math:`V_{\text{ref}} = 50.0 \text{ m/s}`, :math:`V_{\text{avg}} = 10.0 \text{ m/s}`, reference turbulence intensity :math:`I_{\text{ref}} = 0.18`.
  - **Class II**: Medium wind sites, reference speed :math:`V_{\text{ref}} = 42.5 \text{ m/s}`, :math:`V_{\text{avg}} = 8.5 \text{ m/s}`, reference turbulence intensity :math:`I_{\text{ref}} = 0.18`.
  - **Class III**: Low wind sites, reference speed :math:`V_{\text{ref}} = 37.5 \text{ m/s}`, :math:`V_{\text{avg}} = 7.5 \text{ m/s}`, reference turbulence intensity :math:`I_{\text{ref}} = 0.18`.

* **Spectral Scales**:
  The longitudinal scale parameter :math:`\Lambda_u` is defined as a function of height :math:`z`:
  
  .. math::
  
     \Lambda_u = \begin{cases} 0.7 z & \text{if } z < 60\text{ m} \\ 42\text{ m} & \text{if } z \ge 60\text{ m} \end{cases}
  
  The integral length scales for Kaimal and Von Kármán spectra map to :math:`\Lambda_u` via:
  
  .. math::
  
     L_u = 8.1 \Lambda_u, \quad L_v = 2.7 \Lambda_u, \quad L_w = 0.67 \Lambda_u
  
  The target velocity standard deviations are specified by:
  
  .. math::
  
     \sigma_u = \sigma, \quad \sigma_v = 0.8\sigma, \quad \sigma_w = 0.5\sigma

* **Von Kármán Spectrum Formulation**:
  The streamwise (u-component) spectral density is given by (von Kármán, 1948; Panofsky & Dutton, 1984):
  
  .. math::
  
     S_u(f) = \frac{4 L_u \sigma_u^2}{(1 + 70.8 \hat{f}^2)^{5/6}}
  
  where :math:`\hat{f} = \frac{f L_u}{U_{\text{mean}}}` is the normalized frequency.

* **Kaimal Spectrum Formulation**:
  The streamwise (u-component) spectral density is given by (Kaimal et al., 1976):
  
  .. math::
  
     S_u(f) = \frac{4 L_u \sigma_u^2 \hat{f}}{(1 + 6 \hat{f})^{5/3}}
  
  where :math:`\hat{f} = \frac{f L_u}{U_{\text{mean}}}` is the normalized frequency.

* **Coherence Formulations**:
  Cross-component spatial correlations are modeled using directional coherence matrices between velocity components at different heights:
  
  .. math::
  
     \text{Coh}_{ij}(\Delta z, f) = \text{exp}\left( -k \frac{|\Delta z| f}{U_{\text{mean}}} \right)
  
  where :math:`k` is a decay parameter. Supported models include:
  
  - **Gaussian**: :math:`\text{Coh}(\Delta z, f) = \text{exp}(-k \cdot \Delta z^2)`
  - **Exponential**: :math:`\text{Coh}(\Delta z, f) = \text{exp}(-k \cdot |\Delta z|)`
  - **Power-law**: :math:`\text{Coh}(\Delta z, f) = (1 + k \cdot |\Delta z|)^{-m}`

Mann Box Anisotropic Spectral Tensor Model
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The Mann Box model (Mann, 1994; Mann et al., 2016) represents a fully anisotropic 3D turbulent velocity field, capturing sheared spectral tensors and cross-component correlations over complex terrain.

* **Diagonal Spectral Components** (Energy Spectra):
  The energy spectrum for each velocity component :math:`i \in \{u, v, w\}` is defined as:
  
  .. math::
  
     S_{ii}(k) = \frac{8 \sqrt{\frac{3}{11\pi}} \cdot \sigma_i^2 L_i}{k \cdot \left[ 1 + \left( \frac{k L_i}{\alpha} \right)^2 \right]^{5/6}}
  
  where :math:`k` is the wavenumber, :math:`L_i` is the component integral length scale, :math:`\sigma_i^2` is the velocity variance, and :math:`\alpha` is the asymmetry parameter.

* **Off-Diagonal Spectral Components** (Cross-Spectra):
  The cross-spectral components represent cross-correlation between different velocity components (satisfying the Cauchy-Schwarz inequality :math:`|S_{ij}|^2 \le S_{ii} S_{jj}`):
  
  .. math::
  
     S_{ij}(k) = \eta_{ij} \sqrt{S_{ii}(k) S_{jj}(k)} \exp\left( -\left( \frac{k L_{\text{harmonic}}}{300} \right)^2 \right)
  
  where :math:`\eta_{ij}` is the coherence factor (e.g., :math:`\eta_{uv}=0.75, \eta_{uw}=0.50, \eta_{vw}=0.65`) and :math:`L_{\text{harmonic}}` is the harmonic mean scale:
  
  .. math::
  
     L_{\text{harmonic}} = \frac{2 L_i L_j}{L_i + L_j}

* **Terrain Adaptation**:
  In complex terrain, local slopes modify the length scales and spectral components continuously, dynamically scaling energy in the streamwise direction to represent accelerated windward flows and sheared separation over ridge crests.

Terrain-Aware Masking
~~~~~~~~~~~~~~~~~~~~~
To prevent unphysical fluctuation penetration into terrain, a 3D mask is applied to the synthesized fluctuations. The mask :math:`M(z_{\text{agl}})` transitions smoothly from zero inside the terrain to unity far above:

.. math::

   M(z_{\text{agl}}) = \begin{cases}
   0.0 & \text{if } z_{\text{agl}} \le 0 \\
   \frac{1}{2}\left[1 - \cos\left(\frac{\pi z_{\text{agl}}}{h_t}\right)\right] & \text{if } 0 < z_{\text{agl}} < h_t \\
   1.0 & \text{if } z_{\text{agl}} \ge h_t
   \end{cases}

where :math:`h_t` is a transition height (typically 2 to 4 cells tall). The smooth cosine ramp ensures :math:`C^1` continuity at both boundaries, preserving approximate mass conservation.

Advanced Numerical Solver Enhancements
--------------------------------------

These numerical techniques improve accuracy and convergence speed of the AMReX-based multigrid solver.

Divergence Damping Filter
~~~~~~~~~~~~~~~~~~~~~~~~~
After solving the mass-consistency Poisson equation, the Lagrange multiplier field :math:`\lambda` may contain high-frequency noise from discretization. An implicit damping filter is applied:

.. math::

   \lambda_{\text{filtered}} = \lambda - \varepsilon \nabla^2 \lambda

where :math:`\varepsilon = 0.05 \cdot \min(\Delta x, \Delta y, \Delta z)^2` is an automated damping coefficient. This smoothing reduces spurious divergence :math:`\nabla \cdot \mathbf{u}` by 30-50% without affecting physical wind profiles.

Perturbation Pressure Gradient
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To enhance pressure-velocity coupling in high-resolution, complex flow scenarios, an optional perturbation pressure Poisson equation is solved:

.. math::

   \nabla^2 p' = -\nabla \cdot (\mathbf{u} \cdot \nabla \mathbf{u})

And the velocity field is updated:

.. math::

   \mathbf{u}_{\text{corrected}} = \mathbf{u} - \frac{1}{\rho} \nabla p'

This is useful for flow blocking or strong vertical convective accelerations.

Multi-Scale Terrain Analysis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The local topography is classified based on slope magnitude :math:`|\nabla h|`:
* **Flat (:math:`|\nabla h| < 0.1`)**: Standard log-law profile, unperturbed roughness :math:`z_0`.
* **Moderate (:math:`0.1 \le |\nabla h| < 0.3`)**: Aerodynamic roughness length adjusted by factor :math:`(1 + 0.2)`.
* **Steep (:math:`|\nabla h| \ge 0.3`)**: :math:`z_0` adjusted by :math:`(1 + 0.8)`, and subgrid-scale terrain drag parameterization is applied.

Surface-Layer-to-Mixed-Layer Transition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Discontinuities in vertical wind shear at the boundary layer top are eliminated using S-curve smoothstep blending:

.. math::

   u(z) = [1 - w(z)] u_{\text{loglaw}}(z) + w(z) u_{\text{mixed}}(z)

where the weight function :math:`w(z)` blends smoothly over a transition width :math:`h_{\text{blend}}` around the transition height :math:`z_{\text{trans}}`.

Limitations and Future Work
----------------------------

Limitations
~~~~~~~~~~~

1. **Physics**:
   * **Diagnostic Nature**: The solver assumes steady-state mass consistency. Time-dependent fluctuations and transient atmospheric dynamics are parameterized rather than solved prognostically.
   * **Simplified Street Canyon & Obstacles**: While we now explicitly model recirculating street canyon vortices using geometric detection and parameterized solenoidal vortex profiles, canopy drag and basic Oke (1988) algorithms assume homogeneous cell sizes.
   * **Uncoupled Fire-Wind Feedback**: Currently, there is no direct thermal or buoyant feedback from fire fronts (e.g. from wildfire_levelset) back to the wind field.

2. **Numerics**:
   * **Uniform Vertical Resolution**: While horizontal mesh spacing is flexible, vertical grid spacing is constant, making near-surface gradients expensive to resolve.
   * **Interpolation Schemes**: Spatially varying initial conditions rely on local spatial interpolations (such as nearest-neighbor or basic IDW) rather than full high-order 3D trilinear interpolation.

3. **Machine Learning (ML)**:
   * **Surrogate Modeling Absence**: No machine learning surrogates are currently integrated to speed up or replace the multi-level multigrid (MLMG) Poisson solve.

Future Work
~~~~~~~~~~~

1. **Physics**:
   * **Prognostic Coupling**: Integrate dynamic momentum source terms and time-dependent atmospheric forcing.
   * **Active Wind-Fire Feedback**: Implement local sensible heat and buoyancy feedback driven by fire front perimeters to simulate fire-induced winds.
   * **Advanced Canopy Models**: Integrate Leaf Area Density (LAD) vertical profiles and Cionco drag-force parameterizations.

2. **Numerics**:
   * **Stretched Vertical Grids**: Support stretched or terrain-following coordinates with finer grid spacing near the ground surface.
   * **Adaptive Mesh Refinement (AMR)**: Fully utilize AMReX's block-structured AMR capability to dynamically refine the mesh around complex buildings and steep terrain ridges.

3. **Machine Learning (ML)**:
   * **Surrogate ML Solvers**: Integrate deep learning surrogates (e.g., convolutional neural networks or Fourier Neural Operators) trained on Large Eddy Simulation (LES) data to generate extremely fast, highly realistic initial fields.
   * **Physics-Informed Neural Networks (PINNs)**: Explore PINN-based solvers to enforce mass-consistency in non-Cartesian domains, accelerating or replacing traditional multigrid cycles.

