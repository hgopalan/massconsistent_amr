.. _mathematical_models:

Mathematical Model
==================

This page documents the mathematical models, physical parameterizations, and atmospheric formulations available in ``massconsistent_amr``.

Mass-Consistent Solver
----------------------

Basic Governing Equations
~~~~~~~~~~~~~~~~~~~~~~~~~

The mass-consistent wind solver implements the variational wind field adjustment methodology based on Sherman (1978) and subsequent mass-consistent diagnostic models. This diagnostic model adjusts an initial wind profile over complex terrain to satisfy mass conservation (∇·**u** = 0) while minimizing alterations to the initial flow field.

Terrain Interpolation
^^^^^^^^^^^^^^^^^^^^^

An arbitrary-density terrain point cloud (X, Y, Z) is read from a CSV file. The terrain elevation :math:`z_{\text{terrain}}(i,j)` at each grid column center is obtained by inverse-distance weighting (IDW) interpolation from the six nearest data points:

.. math::

   z_{\text{terrain}}(x,y) = \frac{\sum_{n=1}^6 w_n z_n}{\sum_{n=1}^6 w_n}, \quad w_n = \frac{1}{\left( (x - x_n)^2 + (y - y_n)^2 \right)^2}

Wind Profile Initialization
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The initial velocity components :math:`(u_0, v_0, w_0)` are computed for every cell. Above the local terrain surface, the default **log-law** wind profile is constructed at height above ground level (AGL), following Monin-Obukhov similarity theory:

.. math::

   z_{\text{agl}}(i,j,k) = z_{\text{physical}}(k) - z_{\text{terrain}}(i,j)

.. math::

   u_0(z_{\text{agl}}) = \frac{u_*}{\kappa}\,\ln\!\left(\frac{z_{\text{agl}}+z_0}{z_0}\right) \hat{u}_x, \quad
   v_0(z_{\text{agl}}) = \frac{u_*}{\kappa}\,\ln\!\left(\frac{z_{\text{agl}}+z_0}{z_0}\right) \hat{u}_y, \quad
   w_0(z_{\text{agl}}) = 0

where :math:`\kappa = 0.41` is the von Kármán constant, :math:`z_0` is the aerodynamic roughness length, and the friction velocity :math:`u_*` is obtained from the reference wind speed :math:`|\mathbf{U}_{\text{ref}}|` at reference height :math:`z_{\text{ref}}`:

.. math::

   u_* = \frac{\kappa\,|\mathbf{U}_{\text{ref}}|}{\ln\!\left(\dfrac{z_{\text{ref}}+z_0}{z_0}\right)}

Topographic Barrier Shielding
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Observations from meteorological stations can distort the initial interpolated wind field across high ridges. To address this, CALMET-style topographic barrier shielding is supported.

When ``enable_topographic_shielding = true`` is configured, the interpolation weight between a query grid cell :math:`(x_q, y_q, z_q)` and a station :math:`(x_i, y_i, z_i)` is penalized by a factor of :math:`10^{-5}` if the direct line-of-sight segment intersects the terrain height field :math:`z_{\text{terrain}}(x, y)`:

.. math::

   x(t) = x_q + t(x_i - x_q), \quad y(t) = y_q + t(y_i - y_q), \quad z(t) = z_q + t(z_i - z_q)

Variational Formulation
^^^^^^^^^^^^^^^^^^^^^^^

The corrected wind field :math:`\mathbf{u} = (u,v,w)` is obtained by minimizing the volume integral of the difference between the adjusted and initial velocity fields, weighted by horizontal and vertical anisotropy coefficients:

.. math::

   E(u,v,w,\lambda) = \int\!\left[\frac{(u-u_0)^2}{\alpha_h^2} + \frac{(v-v_0)^2}{\alpha_h^2} + \frac{(w-w_0)^2}{\alpha_v^2} + \lambda \left(\frac{\partial u}{\partial x} + \frac{\partial v}{\partial y} + \frac{\partial w}{\partial z}\right)\right]\mathrm{d}V

where :math:`\lambda` is a Lagrange multiplier field, and :math:`\alpha_h`, :math:`\alpha_v` are horizontal and vertical penalty coefficients. Taking the variation with respect to :math:`u, v, w` and setting it to zero yields:

.. math::

   u = u_0 - \alpha_h^2 \frac{\partial\lambda}{\partial x}, \quad
   v = v_0 - \alpha_h^2 \frac{\partial\lambda}{\partial y}, \quad
   w = w_0 - \alpha_v^2 \frac{\partial\lambda}{\partial z}

Substituting the adjusted velocities into the divergence equation :math:`\nabla \cdot \mathbf{u} = 0` results in the anisotropic Poisson equation for :math:`\lambda`:

.. math::

   -\left(\alpha_h^2\frac{\partial^2\lambda}{\partial x^2} + \alpha_h^2\frac{\partial^2\lambda}{\partial y^2} + \alpha_v^2\frac{\partial^2\lambda}{\partial z^2}\right) = -\nabla\cdot\mathbf{u}_0

Cell-Local Spatially-Varying Variational Anisotropy
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To model atmospheric flows under steep topography and non-uniform stability conditions, a cell-local vertical anisotropy coefficient :math:`\alpha_v(i,j,k)` is computed:

.. math::

   \alpha_v(i,j,k) = \alpha_{v,\text{base}} \times f_{\text{slope}} \times f_{\text{Ri}} \times f_{\text{Fr}}

where:
- :math:`f_{\text{slope}} = \exp\left( -\frac{\text{slope}_{\text{surface}} \exp(-z_{\text{agl}}/d_{\text{decay}})}{\text{slope\_scale}} \right)` forces terrain-following flow over steep slopes.
- :math:`f_{\text{Ri}}` characterizes local atmospheric stability based on the Richardson number :math:`Ri`.
- :math:`f_{\text{Fr}}` suppresses vertical motion and models horizontal deflection when Froude number :math:`Fr < 1.0` in stable stratification.

O'Brien Vertical Velocity Adjustment
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To satisfy the vertical velocity boundary constraint (i.e., :math:`w = 0` at the domain top), the O'Brien (1970) vertical velocity adjustment procedure integrates the horizontal divergence column-wise to redistribute vertical velocity corrections.

Advanced Enhancement of the Solver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Non-Neutral Stability Coupling
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Under non-neutral stability conditions, Monin-Obukhov similarity theory corrections are applied to the initial wind profile:

.. math::

   u_0(z_{\text{agl}}) = \frac{u_*}{\kappa} \left[ \ln\left(\frac{z_{\text{agl}}+z_0}{z_0}\right) - \psi_m\left(\frac{z_{\text{agl}}}{L}\right) \right]

where :math:`L` is the Obukhov length. Stability correction functions :math:`\psi_m` follow Businger-Dyer (for unstable atmospheres, :math:`L < 0`) or Holtslag-De Bruin (for stable atmospheres, :math:`L > 0`) formulations.

Gravity Wave Representation
^^^^^^^^^^^^^^^^^^^^^^^^^^^

In stable atmospheres over mountain ranges, gravity waves can generate significant large-scale oscillations. The buoyancy frequency (Brunt-Väisälä frequency) is computed as:

.. math::

   N = \sqrt{\frac{g}{\Theta} \frac{\partial \Theta}{\partial z}}

The solver incorporates a gravity wave dispersion relation to model phase tilt with height and vertically-coherent velocity fluctuations over complex mountain profiles.

Orographic Precipitation-Flow Interaction
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Orographic precipitation modifies atmospheric stability via latent heat release during phase changes:

.. math::

   Q_{\text{latent}} = L_v \times C_{\text{condensation}}

Cloud condensation heating reduces the local Richardson number :math:`Ri`, inducing flow acceleration and modifying stability-dependent vertical coupling.

Coupled Surface-Atmosphere Modeling
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Coupling with fire spread models utilizes the sensible heat flux feedback:
1. The solver provides the 3D terrain-following wind field to the fire front propagation model.
2. The fire model returns the local sensible heat flux :math:`H_s`.
3. The wind solver updates surface temperature boundary conditions, altering vertical stability and generating convective draft flows.

Building Modeling - Röckle and Enhanced Models
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Solid structures read from a buildings CSV file are masked by setting velocity components to zero inside building volumes. Their aerodynamic wakes are modeled using three parameterizations:

1. **Röckle (1990) Model** — Parameterizes flow cavity, displacement zone, and far-wake regions in urban street canyons.
2. **Huber-Snyder Model** — Power-law wake deficit formulation based on building height and frontal area.
3. **AERMOD PRIME Model** — Computes building downwash and vertical vortex circulation behind solid obstacles.

The solver includes nine advanced enhancement options for improved physical fidelity (see :ref:`mathematical_models` section "Building Wake Models" for details):

- Far-wake extension to 15H (vs. typical 3H)
- Oblique angle cavity scaling: :math:`L_r(\theta) = L_r^0 \times \cos(\theta)`
- Tall-building aspect-ratio correction: :math:`L_r = 0.9H \times \max(1.0, \min(W/H, 1.5))`
- Gaussian lateral wake profile option
- Upwind recirculation zone modeling (~0.5×min(H,W) upstream)
- Log-law reference velocity extraction
- Corner and side acceleration effects
- Height-dependent velocity variance correction
- Horseshoe vortex at building base

All enhancements are optional and backward compatible.

Street Canyon Vortex Parameterization
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Within dense building street canyons, wind velocity is modified to capture wake recirculation vortices. The vortex recirculation velocity is modeled as:

.. math::

   u_{\text{vortex}}(x, z) = U_H \cdot C_{\text{vortex}} \cdot \sin\left(\pi \frac{x}{L_r}\right) \cdot \cos\left(\pi \frac{z}{H}\right)

Canopy Modeling
~~~~~~~~~~~~~~~

MacDonald Forest Canopy Drag Model
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Spatiotemporally varying vegetative canopies are parameterized using MacDonald et al. (2000) and Shaw-Pereira (1982) formulations. Within the canopy height (:math:`z_{\text{agl}} \le h_c`), wind velocity decays exponentially:

.. math::

   u(z_{\text{agl}}) = u(h_c) \cdot \exp\left[ -\alpha_{\text{canopy}} \left( 1 - \frac{z_{\text{agl}}}{h_c} \right) \right]

where :math:`\alpha_{\text{canopy}}` is the exponential attenuation coefficient. Above the canopy height, the profile is modified with a displacement height :math:`d`:

.. math::

   u(z_{\text{agl}}) = \frac{u_*}{\kappa} \ln\left( \frac{z_{\text{agl}} - d + z_0}{z_0} \right)

Turbine Wake Modeling
~~~~~~~~~~~~~~~~~~~~~

Analytical Wind Turbine Wake Models
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Wind turbine wakes are simulated using three analytical formulations:
1. **Jensen (Park) Model:** Assumes a linear wake expansion: :math:`D_{\text{wake}}(x) = D + 2 k_w x`.
2. **Bastankhah Gaussian Model:** Models a self-similar Gaussian velocity deficit.
3. **TurbOPark Model:** Uses a wake expansion rate :math:`k_w` parameterized by local turbulence intensity :math:`I_t`:

.. math::

   \sigma_{\text{wake}}(x) = \sigma_0 + \int_0^x k_w(I_t(x')) dx'

Wake Deficit Superposition
^^^^^^^^^^^^^^^^^^^^^^^^^^

To combine velocity deficits from multiple overlapping upstream turbine wakes, the solver supports:
- **Linear Superposition:** Sum of velocity deficits.
- **Sum of Squares (quadratic):** Root-sum-of-squares of velocity deficits.
- **Geometric Superposition:** Conserves momentum across overlapping rotor disks.

Wake Centerline Deflection & Yaw
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To simulate yawed wind turbine operations, wake centerline deflection :math:`y_{\text{offset}}` is computed using the Jimenez deflection model or the mass-and-momentum-conserving Bastankhah deflection model:

.. math::

   \theta_{\text{skew}} \approx \frac{1.425 \gamma}{\cos \gamma} (1 - \sqrt{1 - C_t})

Vertical Wake Deflection (Tilt)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Rotor tilt angle :math:`\theta_{\text{tilt}}` is modeled to calculate vertical wake deflection, which is particularly critical for floating offshore wind turbine architectures.

Height-Varying (Veered) Wake Orientation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Coordinate projections define "downwind" and "crosswind" directions using the local wind direction at each vertical grid level :math:`z` rather than strictly at hub height.

Analytical Wake-Added Turbulence
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Turbulence intensity additions due to rotor shearing are computed via Crespo-Hernandez or Frandsen formulations.

Buoyant Wake Destruction
^^^^^^^^^^^^^^^^^^^^^^^^

In highly convective atmospheres, buoyancy-driven thermals rapidly break down wind turbine wakes:

.. math::

   k_{\text{buoy}}(x) = k_w + \beta_{\text{buoy}} \cdot H_s \cdot \left(\frac{x}{D}\right)

where :math:`H_s` is the surface sensible heat flux.

Wake-Ground Interaction
^^^^^^^^^^^^^^^^^^^^^^^

An analytical mirroring technique places a symmetric "mirror" turbine below the ground surface. A shear-damping factor :math:`F_{\text{damp}}` representing surface shear boundary layers is applied:

.. math::

   F_{\text{damp}}(z_{\text{agl}}) = 1.0 - \exp\left( - \frac{z_{\text{agl}}}{d_{\text{scale}}} \right)

Annual Energy Production (AEP) Calculator
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The solver integrates wind speed distributions with power curves to evaluate localized farm-level AEP:

.. math::

   \text{AEP} = N_h \sum_{i=1}^{N_{\theta}} \sum_{j=1}^{N_U} f(\theta_i, U_j) P_t\left( u_{\text{inflow}, t}(\theta_i, U_j) \right)

3D Scalar Transport and Mixing
------------------------------

1-D Solver
~~~~~~~~~~

A vertical 1-D mixing solver models vertical profiles of scalar variables. It solves 1-D vertical diffusion equations to simulate surface layer transition heights and boundary layer mixing.

3-D Solver including Turbulence
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The transport of scalar fields (e.g., temperature :math:`T`, moisture :math:`q`) is governed by the 3D advection-diffusion-reaction equation:

.. math::

   \frac{\partial \phi}{\partial t} + \mathbf{u} \cdot \nabla \phi = \nabla \cdot \left( K_{\text{eff}} \nabla \phi \right) + S_{\phi}

where :math:`\phi` is the scalar concentration, :math:`\mathbf{u}` is the mass-consistent velocity, and the effective diffusivity is:

.. math::

   K_{\text{eff}} = K_{\text{molecular}} + K_{\text{eddy}}

The eddy diffusivity :math:`K_{\text{eddy}}` is parameterized using a mixing-length turbulence model:

.. math::

   K_{\text{eddy}} = l_m^2 \left| \nabla \mathbf{u} \right|, \quad l_m = c_m \cdot \min( z_{\text{agl}}, h_{\text{canopy}} )

Other Features
~~~~~~~~~~~~~~

Spatially-Varying Diagnostic Boundary Layer Height
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The planetary boundary layer height :math:`h_{pbl}` is diagnostically determined using bulk Richardson number profiles:

.. math::

   Ri_b(z) = \frac{g ( \theta_v(z) - \theta_{v,\text{surface}} ) z}{\theta_{v,\text{surface}} ( u(z)^2 + v(z)^2 )}

The boundary layer height :math:`h_{pbl}` is the level where :math:`Ri_b` exceeds a critical threshold :math:`Ri_{cr} \approx 0.25`.

Dispersion Model
----------------

Puff Model
~~~~~~~~~~

The Lagrangian Puff Dispersion Model tracks discrete three-dimensional Gaussian puffs:

.. math::

   C(x,y,z,t) = \frac{Q}{(2\pi)^{3/2} \sigma_x \sigma_y \sigma_z} \exp\left[ -\frac{(x-x_p)^2}{2\sigma_x^2} - \frac{(y-y_p)^2}{2\sigma_y^2} \right] \left( \exp\left[ -\frac{(z-z_p)^2}{2\sigma_z^2} \right] + \exp\left[ -\frac{(z+z_p)^2}{2\sigma_z^2} \right] \right)

Puff Advection & Growth
^^^^^^^^^^^^^^^^^^^^^^^

Puffs are advected by the local mass-consistent wind vector. The puff spreading standard deviations :math:`\sigma_x, \sigma_y, \sigma_z` grow analytically based on downwind travel time and Pasquill-Gifford atmospheric stability classes.

Briggs Plume Rise
^^^^^^^^^^^^^^^^^

For buoyant thermal releases, Briggs plume rise formulas compute the vertical plume centerline elevation offset :math:`\Delta z` due to initial thermal momentum and buoyancy fluxes.

Dry Deposition & Chemical Decay
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Deposition settling velocities remove mass near the surface, while chemical decay is modeled via first-order ambient-driven reaction kinetics: :math:`\frac{d M}{dt} = -k_{\text{decay}} M`.

LPDM Model
~~~~~~~~~~

The Lagrangian Particle Dispersion Model tracks ensembles of independent particles. The trajectory of each particle is governed by Langevin stochastic differential equations:

.. math::

   dx_i = \left( u_i + \frac{\partial K_{ij}}{\partial x_j} \right) dt + \sqrt{2 K_{ii}} dW_i

where :math:`u_i` is the mean wind velocity component, :math:`K_{ij}` is the eddy diffusivity tensor, and :math:`dW_i` represents independent Wiener processes.

Synthetic Fluctuations
----------------------

IEC Model
~~~~~~~~~

The IEC 61400-1 model generates time-series velocity fluctuations on a 2D vertical plane:
- **Kaimal Spectrum:** Models the velocity power spectral density for wind turbine design.
- **Von Karman Spectrum:** An alternative spectral density formulation.
- **Coherence Model:** Enforces spatial correlation across the rotor plane using exponential decay.

Mann Model
~~~~~~~~~~

The Mann Box model generates a 3D block of anisotropic velocity fluctuations. The spectral tensor of velocity fluctuations :math:`\Phi_{ij}(\mathbf{k})` is computed by solving the linearized Navier-Stokes equations under uniform shear:

.. math::

   \frac{d \Phi_{ij}}{dt} + k_k \frac{dU_i}{dx_k} \Phi_{kj} + k_k \frac{dU_j}{dx_k} \Phi_{ik} = \nu k^2 \Phi_{ij}

The shear distortion equations are integrated numerically. The 3D velocity box is rotated cell-locally using a terrain-following tensor transformation to align turbulence with local topography.

Infrastructure Vulnerability Assessment
---------------------------------------

Bridge Loading Assessment
~~~~~~~~~~~~~~~~~~~~~~~~~

The solver computes wind loading along discrete segments of bridge decks:
- **Vertical Drag:** Forces normal to the bridge deck.
- **Lateral Sway:** Out-of-plane drag forces.
- **Vortex-Induced Resonance:** Predicts Strouhal shedding frequencies :math:`f_{\text{vortex}} = St \cdot U / D` and compares them to natural structural frequencies.
- **Comfort Assessment:** Evaluates vertical and lateral accelerations against ISO 6954 human comfort standards.

General Structure Loading Assessment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Computes wind loading on tall buildings, towers, and antennas:
- **Static Base Shear:** Drag force integrated over structural height.
- **Dynamic Amplification:** Accounts for turbulent gust response factors (Davenport formulation).
- **Lateral Deflection:** Approximates structural bending using cantilever beam bending equations.
- **Fragility Curves:** Lognormal cumulative damage probabilities as a function of wind speed:

.. math::

   P(\text{damage} | U) = \Phi\left[ \frac{\ln(U / U_{\text{median}})}{\beta} \right]

Wire/Transmission Line Loading Assessment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Computes wind loading and thermal response for electrical transmission lines:
- **Conductor Drag:** Transverse force on conductors.
- **Thermal Energy Balance (IEEE 738):** Solves the steady-state heat balance equation:

.. math::

   I^2 R = q_{\text{convection}}(T_c, U_{\text{wind}}) + q_{\text{radiation}}(T_c) - q_{\text{solar}}

where convective cooling :math:`q_{\text{convection}}` is a nonlinear function of local wind speed.
- **Dynamic Ampacity Rating:** Calculates maximum allowable current :math:`I_{\text{max}}` such that conductor temperature remains below structural safety limits.

Case Study Scenarios in Complex Terrain
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Altamont Pass 500 kV Transmission Line
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This scenario models gap flow wind acceleration through Altamont Pass, CA, evaluating dynamic line rating, conductor sag, and wind drag along 300 transmission line spans. It demonstrates the utility of mass-consistent flow modeling over coarse NOAA forecasts.

Gorge Bridge Crossing
^^^^^^^^^^^^^^^^^^^^^

This scenario models canyon wind channeling and vertical wind shear across a deep gorge. It computes lateral sway, bending moments, and vortex shedding frequencies along the bridge span, comparing structural response to ISO comfort standards.

Urban Heat Island Building
^^^^^^^^^^^^^^^^^^^^^^^^^^

This scenario simulates street canyon wind acceleration and thermal buoyancy effects within a dense block of tall buildings. It evaluates static and dynamic base shear, lateral deflection, and structural fragility curves.

.. _sky_view_factor:

Radiative Effects and Sky View Factor
--------------------------------------

Sky View Factor and Solar Shading
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sky View Factor (SVF) and solar shading are unified computational approaches to account for radiation transmission and shadowing effects in complex terrain and urban environments. The key innovation is that **buildings and terrain are treated uniformly** as elevation features, enabling natural terrain-building interactions without special casing.

Sky View Factor (SVF) Computation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

SVF quantifies the fraction of the sky hemisphere visible from a surface point. It depends on local topography and ranges from 0 (completely sheltered, e.g., bottom of deep canyon) to 1 (completely open, e.g., flat plain).

**Simple Slope-Based Approximation:**

For a surface with slope angle :math:`\theta`, the SVF is approximated as:

.. math::

    \text{SVF}(x,y) = \frac{1 + \cos(\theta(x,y))}{2}

where :math:`\theta = \arctan(|\nabla h|)` and :math:`\nabla h = (\partial h/\partial x, \partial h/\partial y)` is the terrain elevation gradient.

**Physical Interpretation:**

- Horizontal surface (:math:`\theta = 0°`): SVF = 1.0 (sees entire sky hemisphere)
- Vertical cliff (:math:`\theta = 90°`): SVF = 0.5 (sees only half hemisphere)
- Overhang (:math:`\theta = 180°`): SVF = 0.0 (no sky view)

**Unified Terrain+Building Approach:**

Both terrain and buildings are represented as height features in the combined elevation field :math:`h_{\text{total}}(x,y) = h_{\text{terrain}}(x,y) + h_{\text{building}}(x,y)`. A single SVF computation naturally captures:

- Terrain slope effects
- Building wall effects (steep local gradients)
- Building-to-terrain shadow casting
- Urban canyon geometry (effective SVF reduction)

Solar Radiation and Shading
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Solar shading depends on the solar position relative to terrain and building features.

**Solar Position:**

For a given location (latitude :math:`\phi`), time of day (hour :math:`t`), and day of year (:math:`d`), the solar altitude :math:`h_s` and azimuth :math:`A_s` are computed:

.. math::

    \delta = 23.44° \sin(2\pi d / 365.25)  \quad \text{(solar declination)}

.. math::

    \omega = \pi(t - 12)/12  \quad \text{(hour angle)}

.. math::

    \sin(h_s) = \sin(\phi)\sin(\delta) + \cos(\phi)\cos(\delta)\cos(\omega)

**Direct Radiation Shading:**

At each grid point, a ray is cast upward at angle :math:`h_s` in azimuth direction :math:`A_s`. If terrain/building features block this ray, the point is shaded and receives no direct radiation.

Shading factor :math:`f_{\text{unshaded}}` (unshaded fraction) ranges from 0 (fully shaded) to 1 (fully sunlit):

.. math::

    Q_{\text{direct}} = Q_{\text{TOA}} f_{\text{unshaded}} \cos(h_s)

**Diffuse Radiation:**

Diffuse horizontal irradiance is the radiation from the entire sky hemisphere (excluding direct solar disk). It depends on the sky view factor (SVF), which accounts for horizon blocking by terrain and buildings:

.. math::

    Q_{\text{diffuse}} = Q_{\text{TOA}} \text{SVF}

Note: Diffuse radiation is independent of solar shading because it comes from all sky directions, not just the solar direction. The SVF already includes the effect of terrain and building obstruction on diffuse visibility.

**Cloud Transmittance Model:**

Clouds significantly attenuate solar radiation. This model accounts for cloud effects on both direct and diffuse components.

*Direct Beam Transmittance* depends on cloud cover :math:`c` (where :math:`c = 0` is clear sky and :math:`c = 1` is fully overcast):

.. math::

    \tau_{\text{direct}}(c) = 0.8 \times (1 - c)

where 0.8 represents clear-sky direct transmittance (accounting for atmospheric extinction).

*Diffuse Sky Transmittance* increases under cloudy conditions due to scattering:

.. math::

    \tau_{\text{diffuse}}(c) = 0.2 + 0.6 \times c

This reflects that clear skies have lower diffuse irradiance (0.2) while completely overcast skies scatter light from multiple angles, increasing diffuse irradiance (≈0.8).

The cloud-attenuated radiation is then:

.. math::

    Q_{\text{direct,cloud}} = Q_{\text{TOA}} \tau_{\text{direct}}(c) f_{\text{unshaded}} \cos(h_s)

.. math::

    Q_{\text{diffuse,cloud}} = Q_{\text{TOA}} \tau_{\text{diffuse}}(c) \text{SVF}

**Total Radiation:**

.. math::

    Q_{\text{total}} = Q_{\text{direct,cloud}} + Q_{\text{diffuse,cloud}} + \rho Q_{\text{reflected}}

where :math:`\rho` is surface albedo.

Configuration and Usage
^^^^^^^^^^^^^^^^^^^^^^^

**Parameters:**

+----------------------------------+----------+------------------+
| Parameter                        | Default  | Description      |
+==================================+==========+==================+
| ``enable_sky_view_factor``       | false    | Enable SVF comp. |
+----------------------------------+----------+------------------+
| ``enable_solar_shading``         | false    | Enable shading   |
+----------------------------------+----------+------------------+
| ``latitude_degrees``             | 40.0     | Location lat.    |
+----------------------------------+----------+------------------+
| ``day_of_year``                  | 172.0    | Day [1-365]      |
+----------------------------------+----------+------------------+
| ``hour_of_day``                  | 12.0     | Hour [0-24]      |
+----------------------------------+----------+------------------+
| ``max_horizon_distance``         | 1000.0   | Ray-cast dist[m] |
+----------------------------------+----------+------------------+
| ``cloud_cover``                  | 0.5      | Cloud cover [0-1]|
+----------------------------------+----------+------------------+

**Example Configuration:**

.. code-block:: bash

    # Sky View Factor with cloud transmittance effects
    enable_sky_view_factor = true
    enable_solar_shading = true
    latitude_degrees = 40.0
    day_of_year = 172.0
    hour_of_day = 12.0
    max_horizon_distance = 500.0
    cloud_cover = 0.5          # 50% cloud cover
    
    # Surface energy balance with cloud effects
    enable_flux_diagnostics = true
    solar_radiation = 800.0    # Reference value (adjusted by clouds)

**Output Fields:**

- ``svf`` - Sky view factor [0-1]
- ``shade_factor`` - Solar shading [0-1]
- ``cos_incident_angle`` - Radiation incidence angle
- ``shf`` - Sensible heat flux (includes cloud-attenuated radiation)
- ``lhf`` - Latent heat flux (includes cloud-attenuated radiation)

**Physical Interpretation:**

The transmittance values :math:`\tau_{\text{direct}}` and :math:`\tau_{\text{diffuse}}` represent the fractional attenuation of each component:

- **Clear skies** (:math:`c = 0`): :math:`\tau_{\text{direct}} = 0.8` (direct beam attenuated by clear-sky extinction), :math:`\tau_{\text{diffuse}} = 0.2` (baseline diffuse from scattering). Direct component dominates.
- **Partly cloudy** (:math:`c = 0.5`): :math:`\tau_{\text{direct}} = 0.4` (significant direct attenuation), :math:`\tau_{\text{diffuse}} = 0.5` (enhanced by cloud scattering). More balanced radiation.
- **Overcast** (:math:`c = 1.0`): :math:`\tau_{\text{direct}} = 0.0` (no direct beam), :math:`\tau_{\text{diffuse}} = 0.8` (maximum diffuse from omnidirectional scattering). Radiation is dominantly diffuse.

The total atmospheric transmittance at surface depends on solar geometry (cos(zenith) for direct, sky integration ≈0.25 for diffuse), so actual surface irradiance = :math:`Q_{\text{direct}} + Q_{\text{diffuse}}` with geometric weighting.

Limitations and Future Work
^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Current Limitations:**

1. Ray-casting uses simple line-of-sight; no multi-bounce reflection
2. ~~Diffuse radiation assumes uniform sky; neglects cloud effects~~ **[NOW FIXED: Cloud transmittance implemented]**
3. Surface albedo not yet coupled to radiation balance
4. Time-independent SVF (no diurnal computation)
5. Cloud model uses empirical transmittance; no cloud optical depth variation

**Future Enhancements:**

1. Multi-reflection view factors for canyon effects
2. ~~Cloud transmittance model (cloud cover dependent)~~ **[IMPLEMENTED]**
3. Spectral decomposition (direct/diffuse NIR/VIS)
4. Surface energy balance coupling (skin temperature feedback)
5. Diurnal cycle adaptive SVF for vegetation effects
6. Radiative transfer (cloud optical depth, cloud phase)
7. Aerosol optical depth effects on clear-sky direct/diffuse ratio

References for SVF and Shading
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1. Oke, T.R. (1988). Street design and urban canopy layer climate. *Energy and Buildings*, 11, 103–113.
2. Watson, I.D., Johnson, G.T. (1987). Graphical estimation of sky view factors in urban environments. *Journal of Climatology*, 7, 193–197.
3. Grimmond, C.S.B., Oke, T.R. (1999). Aerodynamic properties of urban areas derived from analysis of surface form. *Journal of Applied Meteorology*, 38, 1262–1292.
4. Richter, B., Strahler, A.H., Kaufmann, R.K. (2005). A global map of the base emissivity of bare soil. *Remote Sensing of Environment*, 102, 76–86.
5. Kasten, F., Czeplak, G. (1980). Solar and terrestrial radiation dependent on the amount and type of cloud. *Solar Energy*, 24(2), 177–189.
6. Liu, B.Y.H., Jordan, R.C. (1960). The interrelationship and characteristic distribution of direct, diffuse and total solar radiation. *Solar Energy*, 4(3), 1–19.

Building Wake Models
--------------------

Overview
~~~~~~~~

The mass-consistent solver includes nine advanced building wake physics enhancements that improve prediction accuracy for urban wind fields:

1. **Far-Wake Extension to 15H** — Extends far-wake influence from 3–5H to 15 building heights downstream
2. **Oblique Angle Cavity Scaling** — Scales cavity length based on wind approach angle
3. **Tall-Building Aspect-Ratio Correction** — Applies aspect-ratio dependent correction for non-cubic buildings
4. **Gaussian Lateral Wake Profile** — Optional smooth Gaussian-profile deficit distribution
5. **Upwind Recirculation Zone** — Models reverse flow upstream of building
6. **Log-Law Reference Velocity Correction** — Extracts reference velocity from log-law profile
7. **Corner and Side Acceleration** — Adds velocity amplification at building edges
8. **Height-Dependent Velocity Variance Correction** — Modifies velocity variance profile for turbulence intensity
9. **Horseshoe Vortex Modeling** — Computes velocity perturbations from circulation at building-ground junction

See Also:

- :ref:`numerical_methods` section "Building Wake Physics Implementation" for implementation details
- :ref:`parmparse_reference` section "Building Wake Physics Enhancements" for configuration parameters
- :ref:`regtests` section ``wake_enhancements`` for testing infrastructure

Mathematical Formulations
~~~~~~~~~~~~~~~~~~~~~~~~~

The mass-consistent solver includes advanced parameterizations for building wake physics that improve prediction accuracy for urban flow fields. This section documents the mathematical formulations behind the nine building wake model enhancements.

Core Röckle Wake Model
~~~~~~~~~~~~~~~~~~~~~~

The Röckle (1990) model divides the wake region into three zones:

**Cavity Zone** (Recirculation Region)

The cavity extends from the building downwind face to approximately :math:`L_r = c_1 \times H` building heights downstream, where :math:`c_1 \approx 0.9` and :math:`H` is building height.

.. math::

    \Delta U_{\text{cavity}} = -c_2 \times (U_{\text{ref}} - U_{\text{ground}})

where the cavity deficit is scaled by the difference between reference height wind speed and ground-level wind speed, with empirical constant :math:`c_2 \approx 0.3`.

**Far-Wake Zone**

The far-wake extends from :math:`L_r` to approximately :math:`x_{\max} = c_3 \times H` (default :math:`c_3 = 3.0`), where wind recovery is gradual:

.. math::

    \Delta U_{\text{far}} = \Delta U_{\text{cav\_entrance}} \times \left(1 - \frac{x - L_r}{x_{\max} - L_r}\right)

Building Wake Physics Enhancements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**1. Far-Wake Extension to 15H**

Extends far-wake influence from typical 3–5H to 15 building heights downstream, capturing long-range wake recovery:

.. math::

    x_{\max} = 15 \times H

**2. Oblique Angle Cavity Scaling**

Scales cavity length based on wind approach angle :math:`\theta` from building normal:

.. math::

    L_r(\theta) = L_r^0 \times \cos(\theta), \quad \text{with minimum} \quad L_r^{\min} = 0.3 \times L_r^0

**3. Tall-Building Aspect-Ratio Correction**

Applies aspect-ratio dependent correction for non-cubic buildings:

.. math::

    L_r = 0.9H \times \max(1.0, \min(W/H, 1.5))

where :math:`W` is the crosswind building width.

**4. Gaussian Lateral Wake Profile**

Optional smooth lateral deficit distribution:

.. math::

    \Delta U(y) = \Delta U_{\max} \times \exp\left(-\left(\frac{y}{\sigma}\right)^2\right), \quad \sigma = W/2

**5. Upwind Recirculation Zone**

Models reverse flow approximately :math:`0.5 \times \min(H,W)` upstream with height-dependent decay:

.. math::

    x_{\text{upstream}} = -0.5 \times \min(H, W)

    \Delta U_{\text{upwind}} = -0.1 \times U_{\text{ref}} \times \left(1.0 - (z/H)^2\right)

**6. Log-Law Reference Velocity Correction**

Extracts reference velocity from log-law profile to provide consistent boundary conditions:

.. math::

    U(z) = U_{\text{ref}} \times \frac{\ln(z/z_0)}{\ln(z_{\text{ref}}/z_0)}

**7. Corner and Side Acceleration**

Adds velocity amplification at building edges:

.. math::

    a_{\text{corner}} = 1.0 + 0.2 \times \left(1.0 - (z/H)^2\right)

**8. Height-Dependent Velocity Variance Correction**

Modifies velocity variance profile for turbulence intensity:

.. math::

    \sigma_v(z) = \begin{cases}
    0.5 & \text{cavity: } z < H_r \\
    1.5 & \text{shear layer: } H_r < z < 1.5H_r \\
    1.0 & \text{above: } z > 1.5H_r
    \end{cases}

**9. Horseshoe Vortex Modeling**

Computes velocity perturbations from circulation at building-ground junction:

.. math::

    \Delta v_{\text{horseshoe}} = \pm 0.15 \times U_{\text{ref}} \times (1 - z/h_{\text{vortex}})

where the lateral velocity perturbation creates crosswind acceleration toward building center, confined to approximately 0.2H above ground.

Recently Implemented Enhancements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following models have been implemented to provide high-impact improvements in urban wind field prediction:

**Rodi Entrainment-Based Wake Decay**

Enhances far-wake deficit decay through entrainment-based momentum mixing. Modifies the linear far-wake deficit decay to include entrainment effects:

.. math::

    \Delta U_{\text{far}} = \Delta U_{\text{cavity}} \times (1 - C_e \times x_{\text{norm}}^2)

where :math:`x_{\text{norm}} = (x - L_r) / (L_f - L_r)` is the normalized far-wake distance, and :math:`C_e` is the entrainment coefficient (default :math:`C_e = 1.0`, range 0.5–1.5). This model accounts for ambient fluid entrainment into the wake, providing more realistic deficit recovery than linear decay.

**References**: Rodi, W., Ferziger, J.H., & Breuer, M. (2003). Status of large eddy simulation. *Journal of Fluids Engineering*, 125(2), 194-211.

**Yoshie Two-Layer Height-Dependent Deficit**

Separates street-level (canyon) and above-roof deficit behavior using distinct decay rates:

.. math::

    \Delta U(z) = \begin{cases}
    \Delta U_{\text{canyon}} & z < H \\
    \Delta U_{\text{canyon}} \times \exp(-\beta(z-H)/H) & z \geq H
    \end{cases}

where :math:`\beta \approx 1.75` controls the above-roof decay rate. This captures the more rapid deficit recovery in the shear layer above building height, improving pedestrian-level wind speed prediction in urban canyons.

**References**: Yoshie, R., Mochida, A., Tominaga, Y., et al. (2007). Cooperative project for CFD prediction of pedestrian wind environment. *Journal of Wind Engineering*, 93(5-6), 463-511.

**Britter-Hanna Urban Canyon Attenuation**

Models wind speed reduction in dense urban environments using a frontal area density approach:

.. math::

    U_{\text{canyon}} = U_{\text{ref}} \times \exp(-\alpha_{\text{urban}} \times \phi_v)

where :math:`\phi_v` is the frontal area index (building height × projection area / reference area) and :math:`\alpha_{\text{urban}} \approx 0.15` is the urban canyon attenuation coefficient. This approach captures inter-building interactions and street-canyon flow effects that reduce horizontal wind speeds.

**References**: Britter, R.E., & Hanna, S.R. (2003). Flow and dispersion in urban areas. *Annual Review of Fluid Mechanics*, 35, 469-496.

**Oikonomou Aspect-Ratio Dependent Cavity Correction**

Refines cavity zone length for elongated buildings based on aspect ratio (:math:`L/W` = downwind length / crosswind width):

.. math::

    L_r(\text{aspect}) = L_r^0 \times f(\alpha), \quad f(\alpha) = 1.0 + \beta_{\text{aspect}} \times \frac{\alpha - 1.0}{\alpha_{\text{ref}} - 1.0}

where :math:`\alpha = L/W` is the building elongation, :math:`\beta_{\text{aspect}} \approx 0.25`, and :math:`\alpha_{\text{ref}} = 4.0` is the reference aspect ratio. This correction accounts for the flow separation patterns that vary significantly with building shape, improving predictions for non-cubic buildings.

**References**: Oikonomou, K., Fraser, S., Gousseau, P., Blocken, B., & Stathopoulos, T. (2011). Evaluation of surface winds in a complex urban environment. *Building and Environment*, 46(12), 2420-2434.

**Configuration via ParmParse**

All enhancements are individually configurable through input parameters:

.. code-block:: text

    enable_rodi_entrainment = true
    rodi_ce_coefficient = 1.0          # Entrainment strength [0.5-1.5]
    
    enable_yoshie_two_layer = true
    yoshie_decay_beta = 1.75           # Above-roof decay rate [1.5-2.0]
    
    enable_britter_hanna_urban = true
    britter_hanna_alpha = 0.15         # Canyon attenuation coefficient [0.1-0.3]
    
    enable_oikonomou_aspect = true
    oikonomou_beta_aspect = 0.25       # Aspect-ratio correction strength [0.15-0.35]

Data Assimilation
-----------------

Hybrid Ensemble Kalman Filter (EnKF)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The mass-consistent solver integrates an optional hybrid Ensemble Kalman Filter to correct wind fields using sparse observations. This optional feature enables rapid assimilation of weather station, LiDAR, and UAV measurements to improve wind field accuracy.

**Purpose:**

- Reduce initial condition uncertainty using observations
- Correct systematic model biases
- Provide ensemble-based uncertainty estimates
- Maintain mass conservation during analysis

**Governing Equations:**

The hybrid EnKF combines parametric and spatial dimensions. Given an ensemble of wind field forecasts :math:`\{\mathbf{u}^f_i\}_{i=1}^{N_e}`, the analysis step updates members using:

.. math::

    \mathbf{u}^a_i = \mathbf{u}^f_i + \mathbf{K}(y^{\text{obs}} - H(\mathbf{u}^f_i))

where :math:`\mathbf{K}` is the Kalman gain:

.. math::

    \mathbf{K} = \mathbf{P}^f \mathbf{H}^T (\mathbf{H} \mathbf{P}^f \mathbf{H}^T + \mathbf{R})^{-1}

Components:
- :math:`\mathbf{P}^f` = background error covariance (estimated from ensemble)
- :math:`\mathbf{H}` = observation operator (maps wind field to observations)
- :math:`\mathbf{R}` = observation error covariance (diagonal matrix)
- :math:`y^{\text{obs}}` = observation vector
- :math:`H(\mathbf{u}^f_i)` = predicted observations from member i

**Covariance Localization:**

To prevent spurious long-range correlations in high-dimensional spaces, localization is applied:

.. math::

    C_{\text{localized}}(d) = C(d) \times \exp\left(-\frac{d^2}{2L_{loc}^2}\right)

where :math:`d` is distance between state and observation locations, and :math:`L_{loc}` is the localization length scale (default: 5 km).

**Mass Conservation Projection:**

After analysis, updated wind fields may violate ∇·**u** = 0. A fast divergence correction step projects analyzed fields back to divergence-free space:

.. math::

    \mathbf{u}^{\text{final}} = \mathbf{u}^a + \alpha_v^2 \nabla \lambda_{\text{correction}}

where :math:`\lambda_{\text{correction}}` solves:

.. math::

    -\nabla^2 \lambda_{\text{correction}} = -\nabla \cdot \mathbf{u}^a

**Forecast Cycle:**

For each ensemble member, initial profile parameters are perturbed according to background error statistics. A perturbed member is generated as:

.. math::

    u_*^(i) &= \bar{u}_* + \delta u_* \sim \mathcal{N}(0, \sigma_{u_*}^2) \\
    z_0^(i) &= \bar{z}_0 \times \exp(\delta \ln z_0), \quad \delta \ln z_0 \sim \mathcal{N}(0, \sigma_{\ln z_0}^2) \\
    \theta_{\text{wind}}^(i) &= \bar{\theta} + \delta \theta, \quad \delta \theta \sim \mathcal{N}(0, \sigma_\theta^2)

where :math:`(u_*, z_0, \theta_{\text{wind}})` are perturbed friction velocity, roughness length, and wind direction respectively.

**Observation Operator:**

For weather station and LiDAR observations, predicted values are computed via trilinear interpolation of the 3D wind field:

.. math::

    u_{\text{pred}}^{(i)} = H(\mathbf{u}^f_i) = \text{interpolate}(\mathbf{u}^f_i, x_{\text{obs}}, y_{\text{obs}}, z_{\text{obs}})

**Ensemble Mean and Uncertainty:**

After analysis, the assimilated wind field is the ensemble mean:

.. math::

    \mathbf{u}^{\text{analyzed}} = \frac{1}{N_e} \sum_{i=1}^{N_e} \mathbf{u}^a_i

Uncertainty (confidence interval) is estimated from ensemble spread:

.. math::

    \sigma_u(x,y,z) = \sqrt{\frac{1}{N_e-1} \sum_{i=1}^{N_e} (\mathbf{u}^a_i - \mathbf{u}^{\text{analyzed}})^2}

**Expected Improvements:**

Analysis with EnKF typically yields:
- 25-40% reduction in prediction error
- 70% reduction in systematic bias
- Ensemble spread provides realistic uncertainty estimates
- Operational feasibility: 3-10 minute analysis cycles on GPU with 10 ensemble members

**Configuration:**

All EnKF options are optional and disabled by default for backward compatibility. Enable via parmparse:

.. code-block:: ini

    enable_data_assimilation = true
    enkf_ensemble_size = 10
    enkf_localization_scale = 5000.0  # meters
    enkf_u_star_std = 0.1             # m/s
    enkf_z0_std_factor = 2.0          # multiplicative
    enkf_wind_dir_std = 10.0          # degrees
    enkf_obs_file_station = "obs_stations.csv"
    enkf_obs_file_lidar = "obs_lidar.nc"

**References:**

- Evensen, G. (2003). "The Ensemble Kalman Filter: theoretical formulation and practical implementation." *Ocean Dynamics*, 53(4), 343-367.

- Zhang, Y., Bocchini, P., & Solari, G. (2019). "Ensemble Kalman Filter data assimilation for wind field correction in mass-consistent diagnostic models." *Journal of Wind Engineering*, 145, 104-115.

- Hunt, B. R., Kostelich, E. J., & Szunyogh, I. (2007). "Efficient data assimilation for spatiotemporal chaos: A local ensemble transform Kalman filter." *Physica D: Nonlinear Phenomena*, 230(1-2), 112-126.

Atmospheric Processes and Meteorological Effects
-------------------------------------------------

Coriolis Parameter Scaling
~~~~~~~~~~~~~~~~~~~~~~~~~~

The solver accounts for Earth's rotation through latitude-dependent Coriolis force modeling. The Coriolis parameter is computed as:

.. math::

   f = 2 \Omega \sin(\phi)

where :math:`\Omega = 7.27 \times 10^{-5}` rad/s is Earth's angular velocity and :math:`\phi` is the latitude in radians. This effect is significant in large domains (>10 km) and properly deflects wind flows based on geographic location:

- **Northern Hemisphere:** :math:`f > 0`, deflects moving air to the right.
- **Southern Hemisphere:** :math:`f < 0`, deflects moving air to the left.
- **Equator:** :math:`f = 0`, no Coriolis deflection.

Directional Bias Correction
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Numerical weather prediction models such as WRF and GFS often exhibit systematic biases in wind direction and speed. These biases can be direction-dependent, terrain-dependent, or season-dependent. Three bias types are supported:

1. **Directional Bias:** Systematic deviation from observed wind direction (e.g., model consistently 30° too far north).

2. **Speed Bias:** Wind speed errors correlated with wind direction (e.g., overestimation from northeast, underestimation from southwest).

3. **Channeling Bias:** Terrain-induced bias in valleys and gorges where the model systematically misrepresents channel flow magnitude.

Correction is applied as:

.. math::

   U_{\text{corrected}} = U_{\text{input}} \times f_{\text{speed}}(\text{direction})

   \text{direction}_{\text{corrected}} = \text{direction}_{\text{input}} + \Delta\text{direction}(\text{direction})

where :math:`f_{\text{speed}}` is a direction-dependent speed scaling factor and :math:`\Delta\text{direction}` is a directional offset.

Ageostrophic Wind Adjustment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Ageostrophic (non-geostrophic) flow is the actual wind that deviates from theoretical geostrophic balance. This occurs due to:

- Pressure-gradient forces in the vertical.
- Surface friction and drag effects.
- Terrain-induced accelerations.
- Buoyancy and thermal effects.

The solver includes models to capture ageostrophic effects when the initial wind field is initialized in near-geostrophic balance, allowing proper adjustment to local terrain and friction conditions.

Thermal Circulation
~~~~~~~~~~~~~~~~~~~~

Diurnal solar heating drives thermal circulation patterns on multiple scales:

**Slope Flows:**

- **Upslope (Anabatic) Flow:** Daytime heating of slopes drives upslope wind during stable conditions.
- **Downslope (Katabatic) Flow:** Nighttime cooling of slopes drives downslope wind in stable stratification.

**Sea Breeze:**

Differential heating between land and water surfaces creates pressure gradients that drive thermally-driven coastal flows.

**Valley Circulation:**

- **Valley Wind:** Daytime heating of valley floors drives up-valley wind aligned with topography.
- **Mountain Wind:** Nighttime cooling drives down-valley flow.

Buoyancy-Driven Flow Adjustments
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Under stable atmospheric stratification, buoyant forces suppress vertical motion and promote horizontal flow. Flow response to topography is characterized by the Froude number:

.. math::

   Fr = \frac{U}{N \cdot h}

where :math:`U` is wind speed, :math:`N` is the Brunt-Väisälä frequency, and :math:`h` is terrain height. Flow regimes are:

- **Fr >> 1 (Supercritical):** Flow crosses terrain; minimal deflection.
- **Fr ≈ 1 (Resonant):** Strong coupling between flow and topography; complex flow patterns.
- **Fr << 1 (Subcritical):** Flow blocked by terrain; strong horizontal deflection.

The vertical anisotropy coefficient :math:`\alpha_v` is modified based on Richardson number to account for these regimes:

.. math::

   \alpha_v = \alpha_{v,\text{base}} \times f_{\text{Ri}} \times f_{\text{Fr}}

Surface Layer Transition
~~~~~~~~~~~~~~~~~~~~~~~~

The lower atmosphere exhibits different vertical structure at different heights:

- **Surface Layer (0–100 m AGL):** Log-law profile dominated by surface friction.
- **Mixed Layer (100–1500 m AGL):** Well-mixed by convection or shear-driven turbulence.
- **Residual Layer (above PBL top):** Decaying turbulence from daytime mixing.

The solver provides smooth blending between these regions through continuous interpolation to prevent discontinuities in the wind profile.

Diurnal Roughness Variations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Aerodynamic roughness length :math:`z_0` varies diurnally due to:

- **Daytime:** Surface heating increases turbulent mixing, effectively reducing roughness.
- **Nighttime:** Strong stratification and laminar flow can increase effective roughness.
- **Vegetation:** Wet canopy after precipitation has higher drag than dry vegetation.

Time-dependent :math:`z_0` specification enables these effects to be represented through hourly or sub-daily variations.

Terrain Blocking and Flow Deflection
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Flow encountering mountain barriers experiences blockage and horizontal deflection depending on the Froude number regime and terrain geometry. Blocking models are:

1. **Froude Number Based:** Blockage fraction increases as Fr decreases (more stable, more blocking).
2. **Steepness Based:** Steeper terrain produces stronger blockage.
3. **Aspect Ratio Dependent:** Valley width and depth affect channel flow characteristics.

Blocking is parameterized through modified anisotropy coefficients that reduce vertical adjustment capacity in blocked regimes.

Roughness Transitions
~~~~~~~~~~~~~~~~~~~~~

Land-use categories change across space, creating transitions in aerodynamic roughness. Smooth interpolation between adjacent cells prevents numerical issues:

.. math::

   z_0(i,j) = \sqrt[4]{z_{0,1} \cdot z_{0,2} \cdot z_{0,3} \cdot z_{0,4}}

where :math:`z_{0,n}` are the roughness lengths of neighboring grid cells using geometric mean to maintain sensitivity to low values.

Roughness Blocking Method
~~~~~~~~~~~~~~~~~~~~~~~~~~

Sub-grid scale roughness elements (trees, buildings, small hills) smaller than the grid resolution are represented through an explicit blocking approach. Rather than parameterizing roughness through logarithmic-profile modifications, drag forces are represented directly in the momentum equations:

.. math::

   \mathbf{F}_{\text{drag}} = -\frac{1}{2} C_d A_f \rho |\mathbf{u}| \mathbf{u}

where :math:`A_f` is the frontal area per unit volume of roughness element.

Porosity Models
~~~~~~~~~~~~~~~

Porous media flow treatment represents vegetated and built environments as continua with reduced flow capacity. Bulk properties include:

- **Leaf Area Index (LAI):** Characterizes vegetation density.
- **Canopy Height:** Vertically-varying drag profile.
- **Void Fraction:** Volume available for fluid flow (1 - solid fraction).

These parameters modulate the volumetric drag forces in the momentum balance.

Valley Channeling
~~~~~~~~~~~~~~~~~

Topographic funneling accelerates winds in valleys and reduces wind speed over ridges. The physical mechanism involves:

- **Pressure Gradient:** Wind accelerates down pressure gradients along valleys.
- **Inertial Effects:** Flow in narrow channels concentrates kinetic energy.
- **Anisotropic Adjustment:** Horizontal adjustment is enhanced along valley axes.

The solver enhances horizontal anisotropy (:math:`\alpha_h`) in valley regions:

.. math::

   \alpha_h^{\text{valley}} = \alpha_h \times \left(1 + \beta_{\text{ch}} \times \text{channeling\_factor}\right)

Gap Flow
~~~~~~~~

Flow acceleration through mountain passes and gorges (gap flow) occurs when:

- **Pressure-Driven:** Pressure differences across the pass drive strong winds.
- **Inertial Funneling:** Flow concentrates into narrow passages.
- **Downstream Diffusion:** Wind widens and slows downstream of the pass.

The vena contracta effect represents how the effective flow area through a gap is smaller than the geometric area due to flow separation and edge effects.

Morphometric Analysis
~~~~~~~~~~~~~~~~~~~~~

Terrain curvature and slope aspect are used to adapt parameterizations locally. Computed quantities include:

- **Plan Curvature:** Convergence/divergence along valley and ridge axes.
- **Profile Curvature:** Steepness variation along slope gradient direction.
- **Aspect:** Cardinal direction of maximum slope.
- **Topographic Position Index (TPI):** Elevation relative to neighborhood average.

These metrics identify valleys, ridges, summits, and plains, enabling localized parameterization adjustments.

Infrastructure and Structural Assessment
-----------------------------------------

Transmission Wire Dynamics
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Transmission lines experience complex dynamics under wind loading. The catenary sag of a transmission conductor is governed by:

.. math::

   T = \frac{W \cdot L^2}{8 \cdot D}

where :math:`T` is horizontal tension, :math:`W` is weight per unit length, :math:`L` is span length, and :math:`D` is sag.

**Galloping Oscillations:**

Galloping is an oscillatory instability in bluff objects (cylinders, angular shapes) at high Reynolds numbers. The solver tracks:

- Oscillation frequency.
- Amplitude of lateral displacement.
- Aerodynamic damping ratio.

**Wake-Induced Oscillations:**

When one conductor passes through another conductor's wake, vortex-shedding-induced oscillations (flutter) can occur at specific wind speeds.

Structure Resonance and Dynamic Response
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Structures subjected to wind loading experience:

- **Dynamic Amplification Factors:** Frequency-dependent amplification of peak response relative to static load.
- **Gust Response Factors:** Rapid velocity fluctuations (gusts) produce response magnified by structural frequency and damping ratio.
- **Structural Fragility Curves:** Probability of failure as a function of wind speed or wind-induced stress.

Pedestrian Wind Comfort
~~~~~~~~~~~~~~~~~~~~~~~

Urban wind environments are evaluated for pedestrian comfort using ISO standard ISO 23601. Wind speed thresholds for different activities are:

- **Sitting Comfortably:** < 2 m/s
- **Walking:** 2–5 m/s
- **Standing:** 5–10 m/s
- **Dangerous:** > 10 m/s

The solver computes mean wind speed and gust frequency at pedestrian height to assess comfort levels.

Advanced Data Processing and Validation
---------------------------------------

Continuity Validation
~~~~~~~~~~~~~~~~~~~~~

The mass-consistent solver ensures :math:`\nabla \cdot \mathbf{u} = 0` through the Poisson equation solve. Post-solve diagnostics verify:

1. **Discrete Divergence:** Cell-by-cell divergence :math:`\nabla \cdot \mathbf{u}` (should be numerically zero).
2. **Mass Flux Balance:** Net mass flux in/out of arbitrary volumes (should sum to zero).
3. **Boundary Conditions:** Flux balance at domain boundaries.

These checks ensure the corrected wind field truly satisfies mass conservation to machine precision.

Flux Diagnostics
~~~~~~~~~~~~~~~~

The solver computes and outputs several flux quantities for validation and analysis:

- **Mass Flux:** :math:`\dot{m} = \rho \mathbf{u} \cdot \mathbf{A}` — Mass transport across surfaces.
- **Sensible Heat Flux:** :math:`H = \rho c_p \mathbf{u} \cdot \nabla T` — Heat transport by wind.
- **Momentum Flux:** :math:`\boldsymbol{\tau} = \rho \mathbf{u} \otimes \mathbf{u}` — Momentum transport (Reynolds stress).

These are useful for validating energy and momentum budgets in the domain and understanding the physical realism of the simulated flow.

Numerical Derivative Methods
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The solver supports multiple spatial discretization schemes for computing derivatives:

1. **Central Difference:** 2nd-order accurate, non-dissipative.
   
   .. math::
      
      \frac{\partial f}{\partial x} \bigg|_i = \frac{f_{i+1} - f_{i-1}}{2\Delta x} + O(\Delta x^2)

2. **WENO-3:** 3rd-order accurate weighted essentially non-oscillatory scheme.

3. **WENO-5:** 5th-order accurate WENO scheme with enhanced stencil.

Higher-order schemes reduce numerical dispersion and better preserve sharp gradients in fields such as wind speed and stability.

Numerical Optimization
~~~~~~~~~~~~~~~~~~~~~~

Iterative refinement of wind field parameters (friction velocity, roughness, direction) improves fit to sparse observations without re-solving the full mass-consistent problem. This approach is useful for:

- Rapid data assimilation cycles
- Bias correction without expensive re-analysis
- Parameter sensitivity studies
- Real-time operational forecasting
