Numerical Method
================

This section describes the numerical methods, discretization schemes, and computational algorithms implemented in the massconsistent_amr solver.

Mass-Consistent Solver
----------------------

Anisotropic Poisson Equation Discretization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The core mass-consistency adjustment is formulated as a system of partial differential equations solved on a three-dimensional terrain-following, cell-centered grid. The anisotropic Poisson equation for the Lagrange multiplier field :math:`\lambda` is discretized using a second-order finite difference scheme:

.. math::

   - \left( \alpha_h^2 \frac{\partial^2 \lambda}{\partial x^2} + \alpha_h^2 \frac{\partial^2 \lambda}{\partial y^2} + \alpha_v^2 \frac{\partial^2 \lambda}{\partial z^2} \right) = - \nabla \cdot \mathbf{u}_0

On the grid, the spatial derivatives are approximated using second-order central differences:

.. math::

   \frac{\partial^2 \lambda}{\partial x^2} \approx \frac{\lambda_{i+1,j,k} - 2\lambda_{i,j,k} + \lambda_{i-1,j,k}}{\Delta x^2}

where :math:`\Delta x`, :math:`\Delta y`, and :math:`\Delta z` represent the grid spacings in the respective directions.

AMReX MLMG Linear Solver
~~~~~~~~~~~~~~~~~~~~~~~~

The discrete anisotropic Poisson system is solved using the **AMReX Multi-Level MultiGrid (MLMG)** linear solver framework via the ``MLABecLaplacian`` operator class. 

Key computational aspects include:
- **Multigrid Cycles:** V-cycles are utilized to rapidly damp low-frequency errors on coarser representation levels.
- **Smoothers:** Red-Black Gauss-Seidel relaxation is applied as the default smoother.
- **Bottom Solvers:** Conjugate Gradient (CG) or Biconjugate Gradient Stabilized (BiCGStab) methods are configured to solve the coarsest grid level.
- **Convergence Criteria:** The solver terminates when the maximum absolute residual or the relative residual falls below user-specified tolerances:

  .. code-block:: ini

     # Controlling multigrid convergence
     solver_tolerance = 1.0e-11
     solver_relative_tolerance = 1.0e-11
     solver_max_iterations = 100

Numerical Boundary Conditions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Physical boundary conditions are mapped directly to the algebraic system using ghost cells:
- **x-faces (Inflow/Outflow):** Dirichlet boundary condition :math:`\lambda = 0`.
- **y-faces (Lateral Boundaries):** Neumann boundary condition :math:`\frac{\partial \lambda}{\partial y} = 0`.
- **z-faces (Ground Surface & Domain Top):** Neumann boundary condition :math:`\frac{\partial \lambda}{\partial z} = 0`.

Divergence Damping Filter
~~~~~~~~~~~~~~~~~~~~~~~~~

To filter out high-frequency spatial noise introduced by terrain-following transformations, an implicit Laplacian smoothing filter is applied to the Lagrange multiplier field:

.. math::

   \lambda_{\text{filtered}} = \lambda - \varepsilon \nabla^2 \lambda

where the smoothing parameter is automatically scaled as:

.. math::

   \varepsilon = 0.05 \cdot \min(\Delta x, \Delta y, \Delta z)^2

Perturbation Pressure Gradient
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In high-resolution or high-shear complex flows, the perturbation pressure Poisson equation is solved:

.. math::

   \nabla^2 p' = -\nabla \cdot (\mathbf{u} \cdot \nabla \mathbf{u})

Velocity corrections are computed as:

.. math::

   \mathbf{u}_{\text{corrected}} = \mathbf{u} - \frac{1}{\rho} \nabla p'

3D Scalar Transport and Mixing
------------------------------

1-D Solver Discretization
~~~~~~~~~~~~~~~~~~~~~~~~~

Vertical mixing and 1-D vertical diffusion are discretized on the 1D column layout using a localized finite-difference scheme. This vertical discretization allows rapid calculation of atmospheric surface layer profiles.

3-D Solver with Mixing Length Turbulence
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The full 3-D transport equation for scalar fields (such as temperature or moisture) is discretized over the AMReX mesh:

.. math::

   \frac{\partial \phi}{\partial t} + \nabla \cdot (\mathbf{u} \phi) = \nabla \cdot (K_{\text{eff}} \nabla \phi)

The numerical schemes employed are:
1. **Advection Scheme:** First-order conservative upstream differencing to guarantee strict positivity and avoid spurious oscillations.
2. **Diffusion Scheme:** Second-order central differences to approximate the spatial gradients of the effective eddy diffusivity :math:`K_{\text{eff}}`.
3. **Time Integration:** Explicit Forward Euler integration under adaptive time-stepping constraints.

Adaptive CFL Time Stepping
~~~~~~~~~~~~~~~~~~~~~~~~~~

The advection-diffusion system is advanced using a dynamically calculated time step :math:`\Delta t` satisfying the Courant-Friedrichs-Lewy (CFL) stability criterion:

.. math::

   \Delta t = \text{CFL} \cdot \frac{\min(\Delta x, \Delta y, \Delta z)}{\max(|u|, |v|, |w|)}

where the user-specified CFL parameter (typically 0.8) ensures strict numerical stability.

Dispersion Model
----------------

Building Wake Model Discretization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The building wake models are implemented as local velocity deficit functions evaluated at each grid point. The Röckle wake model divides the wake region into three zones with distinct deficit profiles:

1. **Cavity Zone Discretization:** Points within :math:`0 < x_{\text{wake}} < L_r` and :math:`|y_{\text{wind}}| < W/2` experience recirculation deficit. The deficit magnitude is computed as:

   .. math::

      \Delta U_{\text{cavity}} = -c_2 \times U_{\text{ref}} \times \text{deficit\_scale}

   where :math:`c_2 \approx 0.3` is the empirical cavity deficit coefficient.

2. **Far-Wake Zone Discretization:** Points in the range :math:`L_r < x_{\text{wake}} < L_f` are subject to linear (or entrainment-based) deficit decay. The normalized distance in the far-wake is computed as:

   .. math::

      x_{\text{normalized}} = \frac{x_{\text{wake}} - L_r}{L_f - L_r}

   The default linear decay factor is then modified by the Rodi entrainment model if enabled:

   .. math::

      \text{decay\_factor} = 1.0 - C_e \times x_{\text{normalized}}^2

   where :math:`C_e \approx 1.0` (default) is the entrainment coefficient.

3. **Height-Dependent Deficit Modification:** Above the building height (:math:`z > H`), the Yoshie two-layer model applies exponential decay to the deficit:

   .. math::

      \Delta U(z) = \Delta U_{\text{base}} \times \exp(-\beta(z - H)/H)

   where :math:`\beta \approx 1.75` controls the above-roof decay rate.

**Grid Operations:**

- All deficit calculations are performed at the cell-center locations on the AMReX MultiFab grid.
- Wind-aligned coordinates are computed from building geometry and local wind direction using rotation matrices.
- Velocity deficits are superposed across overlapping wakes using distance-weighted blending to ensure smooth field transitions.

Urban Canyon Density Effects
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For building ensembles in dense urban environments, the Britter-Hanna urban canyon attenuation model reduces wind speeds based on local building density:

.. math::

   \phi_v = \frac{A_{\text{frontal}} \times H}{A_{\text{reference}}}

The wind speed reduction factor is applied as:

.. math::

   U_{\text{attenuated}} = U_{\text{original}} \times \exp(-\alpha_{\text{urban}} \times \phi_v)

where :math:`\alpha_{\text{urban}} \approx 0.15` is the urban canyon attenuation coefficient.

Aspect-Ratio Corrections
~~~~~~~~~~~~~~~~~~~~~~~~

For non-cubic buildings, the Oikonomou aspect-ratio dependent cavity correction modifies the cavity length based on the building elongation:

.. math::

   L_r(\text{corrected}) = L_r^{\text{base}} \times \left(1.0 + \beta_{\text{aspect}} \frac{\alpha - 1.0}{\alpha_{\text{ref}} - 1.0}\right)

where :math:`\alpha = L/W` is the building aspect ratio and :math:`\beta_{\text{aspect}} \approx 0.25`.

Puff Model Numerical Tracking
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Lagrangian Puff Dispersion Model tracks discrete three-dimensional Gaussian puffs:
- **Velocity Interpolation:** Trilinear interpolation is used to project the 3D wind velocity from cell centers to the exact continuous coordinates of each puff center :math:`(x_p, y_p, z_p)`.
- **Advection Integration:** An explicit Euler step advances the puff coordinates:

  .. math::

     \mathbf{x}_p(t + \Delta t) = \mathbf{x}_p(t) + \mathbf{u}(\mathbf{x}_p) \Delta t

- **Analytical Growth:** Puff standard deviations :math:`\sigma_x, \sigma_y, \sigma_z` are updated analytically at each step based on the travel time and local atmospheric stability class.

LPDM Model Numerical Random Walk
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Lagrangian Particle Dispersion Model tracks large ensembles of independent computational particles.
- **Stochastic Differential Equation:** Particle coordinates are integrated via a drift-diffusion random walk (Langevin equation):

  .. math::

     x_i(t + \Delta t) = x_i(t) + \bar{u}_i(x) \Delta t + \sqrt{2 K_{ii} \Delta t} \cdot \xi

  where :math:`\xi` represents a standard normal random variable generated using a high-quality pseudorandom number generator initialized by ``lpdm_random_seed``.

- **Interpolation:** Particle local wind speeds :math:`\bar{u}_i` and eddy diffusivities :math:`K_{ii}` are interpolated using local trilinear shape functions from the cell-centered AMReX MultiFab grid.

Synthetic Fluctuations
----------------------

IEC Model Spectral Synthesis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The IEC 61400-1 model generates time-series velocity fluctuations on a 2D vertical rotor plane:
- **Frequency-Domain Synthesis:** Velocity components in the frequency domain are constructed by scaling random complex variables with the Kaimal or Von Karman power spectral density curves.
- **Inverse Fast Fourier Transform (IFFT):** A 1D Fast Fourier Transform maps frequency-domain components to time-series fluctuations at each grid point.
- **Spatial Coherence:** Cross-spectral density is enforced by applying a coherence decay function:

  .. math::

     Coh(r, f) = \exp\left[ -a \left(\frac{f \cdot r}{U_{\text{hub}}}\right)^b \right]

Mann Model Box Generation
~~~~~~~~~~~~~~~~~~~~~~~~~

The Mann model generates a three-dimensional block of anisotropic velocity fluctuations:
- **Spectral Tensor Discretization:** The Mann anisotropic spectral tensor :math:`\Phi_{ij}(\mathbf{k})` is evaluated on a uniform 3D wavenumber grid :math:`(k_x, k_y, k_z)`.
- **Shear Distortion Integration:** The linear shear distortion equations are integrated numerically using adaptive quadrature to compute the spectral tensor coefficients.
- **3D Fast Fourier Transform:** Random complex Gaussian variables colored by the Mann tensor are transformed via a 3D IFFT to obtain a spatial box of velocity fluctuations.
- **Grid Mapping & Rotation:** The flat Mann box fluctuations are projected onto the terrain-aligned coordinate system using a localized tensor rotation algorithm.

Infrastructure Vulnerability Assessment
----------------------------------------

Discretized Wind Loading Integration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To evaluate loading on bridges, tall buildings, and wire spans, the continuous geometries are represented as discrete segment segments:
- **Segment Partitioning:** Spans are partitioned into :math:`M` segments of length :math:`\Delta s`.
- **Velocity Interpolation:** The local wind vector :math:`(u, v, w)` is interpolated from the nearest cell centers of the 3D AMReX MultiFab grid.
- **Discrete Force Summation:** Static base shear force and overturning moments are computed via summation:

  .. math::

     F_{\text{total}} = \sum_{j=1}^M \frac{1}{2} \rho C_d A_j |\mathbf{U}_j|^2

Conductor Dynamic Thermal Rating Solver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Overhead electrical wires are assessed for thermal sag and dynamic ampacity using a steady-state heat balance solver:
- **Nonlinear Conduction Solver:** A numerical root-finder solves the nonlinear IEEE 738 energy balance equation:

  .. math::

     I^2 R - h(T_c - T_a) - \varepsilon \sigma (T_c^4 - T_a^4) - q_{solar} = 0

  for the conductor temperature :math:`T_c` under the influence of wind-speed dependent convective cooling :math:`h = f(\mathbf{u}_{\text{local}})`.

- **Dynamic Ampacity Extraction:** Given a maximum allowable temperature threshold :math:`T_{c,\text{max}}`, the solver analytically extracts the maximum allowable current :math:`I_{\text{max}}` (ampacity).
