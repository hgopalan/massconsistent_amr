.. _mathematical_models:

Mathematical Model
==================

This page documents the mathematical models, physical parameterizations, and atmospheric formulations available in ``massconsistent_amr``.

Mass-Consistent Solver
----------------------

Basic Governing Equations
~~~~~~~~~~~~~~~~~~~~~~~~~

The mass-consistent wind solver implements the variational wind field adjustment methodology based on Sherman (1978) and Mathiesen (1987). This diagnostic model adjusts an initial wind profile over complex terrain to satisfy mass conservation (∇·**u** = 0) while minimizing alterations to the initial flow field.

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

Building Modeling - Röckle and other models
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Solid structures read from a buildings CSV file are masked by setting velocity components to zero inside building volumes. Their aerodynamic wakes are modeled using three parameterizations:

1. **Röckle (1990) Model** — Parameterizes flow cavity, displacement zone, and far-wake regions in urban street canyons.
2. **Huber-Snyder Model** — Power-law wake deficit formulation based on building height and frontal area.
3. **AERMOD PRIME Model** — Computes building downwash and vertical vortex circulation behind solid obstacles.

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
