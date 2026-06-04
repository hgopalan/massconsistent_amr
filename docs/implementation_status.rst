.. _implementation_status:

Mann Model and IEC 61400 Implementation
=======================================

This section details the physical models, numerical formulations, and validated capabilities of the mass-consistent wind solver's turbulence and spectral models: the **Mann Box Model** and the **IEC 61400-1 Standard**.

----

Mann Box Model (Synthetic Turbulence)
------------------------------------

The synthetic turbulence system generates time-resolved, terrain-aware anisotropic velocity fluctuations based on the Mann spectral tensor formulation.

Spectral Tensor Structure
~~~~~~~~~~~~~~~~~~~~~~~~~

The anisotropic spectral tensor represents turbulence as a 3×3 symmetric positive semi-definite matrix:

.. math::

   \mathbf{S}(\mathbf{k}) = \begin{bmatrix}
   S_{uu}(\mathbf{k}) & S_{uv}(\mathbf{k}) & S_{uw}(\mathbf{k}) \\
   S_{vu}(\mathbf{k}) & S_{vv}(\mathbf{k}) & S_{vw}(\mathbf{k}) \\
   S_{wu}(\mathbf{k}) & S_{wv}(\mathbf{k}) & S_{ww}(\mathbf{k})
   \end{bmatrix;

Key properties enforced by the solver include:
* **Symmetry**: :math:`S_{ij} = S_{ji}`, which reduces storage to 6 unique components.
* **Positive Semi-Definiteness**: All eigenvalues :math:`\lambda \ge 0`.
* **Cauchy-Schwarz Inequality**: :math:`|S_{ij}|^2 \le S_{ii} \times S_{jj}` for all component pairs.
* **Energy Non-Negativity**: Diagonal energy spectra :math:`S_{ii}(\mathbf{k}) \ge 0`.

Diagonal Components (Energy Spectra)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The diagonal spectral components representing energy distribution across wavenumbers are modeled as:

.. math::

   S_{ii}(k) = \frac{8\sqrt{3/11\pi} \cdot \sigma_i^2 \cdot L_i}{k \cdot \left[1 + \left(k \cdot L_i / C\right)^2\right]^{5/6}}

where :math:`k` is wavenumber magnitude, :math:`\sigma_i^2` is component variance, :math:`L_i` is the integral length scale, and :math:`C` is an asymmetry parameter (typically 1.0). At high wavenumbers, energy decays following the physical Kolmogorov :math:`-5/3` power law (or :math:`-5/6` in 1D wave space).

Off-Diagonal Components (Cross-Spectra)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Cross-spectra components representing spatial coherence and correlation between wind components are modeled as:

.. math::

   S_{ij}(k) = \eta_{ij} \cdot \sqrt{S_{ii} \cdot S_{jj}} \cdot \exp\left(-\left(k \cdot L_{\text{harmonic}} / \alpha\right)^2\right)

where :math:`\eta_{ij} \in [0, 1]` is the coherence factor, :math:`L_{\text{harmonic}} = 2 L_i L_j / (L_i + L_j)` is the harmonic mean of the component scales, and :math:`\alpha` is the spatial decorrelation scale.

Temporal & Stability Physics
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Time-Lag Autocorrelations
^^^^^^^^^^^^^^^^^^^^^^^^^

The fixed-point temporal correlation decays with time separation :math:`\tau` according to Eulerian or Lagrangian frameworks:

* **Eulerian Autocorrelation**: :math:`\rho_E(\tau) = \exp(-|\tau|/T_{\text{int}})` or :math:`\exp(-(\tau/T_{\text{int}})^2)` where :math:`T_{\text{int}}` is the integral timescale.
* **Lagrangian Autocorrelation**: :math:`\rho_L(\tau) = \rho_E(\tau/2)`, reflecting that moving fluid particles experience faster decorrelation.

Under the Taylor frozen turbulence hypothesis, the spatial-temporal correlation evolves as:

.. math::

   R_{ij}(\mathbf{r}, \tau) \approx R_{ij}(\mathbf{r} - \mathbf{U}_h \cdot \tau, 0)

assuming the turbulence pattern convects undistorted with the mean horizontal wind :math:`\mathbf{U}_h`.

Stability Classification
^^^^^^^^^^^^^^^^^^^^^^^^

Atmospheric stability is quantified by the bulk Richardson number (:math:`Ri_b`) and the Obukhov length (:math:`L_{MO}`):

.. math::

   Ri_b = \frac{g}{\theta} \cdot \frac{\partial \theta / \partial z \cdot z^2}{(\partial u / \partial z)^2 + (\partial v / \partial z)^2}

* :math:`Ri_b > 0.25`: **Stable** regime (stratification suppresses turbulence).
* :math:`-0.25 \le Ri_b \le 0.25`: **Neutral** regime (shear-dominated).
* :math:`Ri_b < -0.25`: **Unstable** regime (buoyancy-enhanced).

The Obukhov length is calculated from heat flux :math:`H_s`:

.. math::

   L_{MO} = -\frac{u_*^3 \cdot T_0}{\kappa \cdot g \cdot (H_s / \rho c_p)}

Stability Modifications on Spectral Tensor
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Spectral tensor components are modified by stability-dependent factors :math:`f_i(Ri_b)`:
* **Stable conditions**: Vertical fluctuations are suppressed while horizontal ones are enhanced:
  
  .. math::
  
     f_w = 0.5 - 0.5 \cdot Ri_b \quad (\ge 0.1), \qquad f_{u,v} = 1.0 + 0.3 \cdot Ri_b \quad (\le 1.5)

* **Unstable conditions**: Vertical fluctuations are enhanced due to thermal plumes:
  
  .. math::
  
     f_w = 1.0 - 0.5 \cdot Ri_b \quad (\le 2.0), \qquad f_{u,v} = 1.0 + 0.2 \cdot Ri_b \quad (\ge 0.5)

In unstable conditions, the convective velocity scale :math:`w_* = (g/\theta \cdot H_s \cdot \delta)^{1/3}` is utilized to scale mixing.

Terrain Adaptation & Flow Regimes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Flow Regime Classification
^^^^^^^^^^^^^^^^^^^^^^^^^^

Local topography induces spatial variation in wind fields. The solver automatically classifies five distinct flow regimes:
1. **Neutral**: Well-mixed standard boundary layer.
2. **Acceleration**: Streamwise compression on windward slopes (:math:`\nabla \cdot \mathbf{u} > \text{threshold}`).
3. **Separation**: Recirculating flow and vortex shedding on lee sides (:math:`|\nabla \times \mathbf{u}| > \text{threshold}`).
4. **Stagnation**: Flow blockages in pockets or deep valleys (:math:`|\mathbf{u}| \approx 0`).
5. **Channeling**: Flow alignment with valley axes.

Slope-Aware Tensor Rotation
^^^^^^^^^^^^^^^^^^^^^^^^^^^

To maintain physical alignment with the terrain, the anisotropic spectral tensor is rotated into a slope-aligned coordinate system :math:`(s, n, \hat{z})` representing along-slope, cross-slope, and surface-normal components, respectively:

.. math::

   \mathbf{S}_{\text{rotated}} = \mathbf{R} \cdot \mathbf{S} \cdot \mathbf{R}^T

where :math:`\mathbf{R} = \mathbf{R}_{\text{slope}} \times \mathbf{R}_{\text{azimuth}}` is the 3D rotation matrix constructed from local topography elevation gradients.

Multi-Scale Terrain Cascade
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Terrain features are decomposed into small-scale, medium-scale, and large-scale contributions. Scale-specific adaptation factors are weighted based on height above ground and combined geometrically to conserve turbulent energy.

Boundary Layer (BL) Regions
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The boundary layer height :math:`\delta` defines three physical regions:
* **Surface Layer** (:math:`z < 0.1\delta`): Dominated by surface shear and roughness.
* **Mixed Layer** (:math:`0.1\delta \le z < \delta`): Dominated by convective/turbulent mixing.
* **Free Atmosphere** (:math:`z \ge \delta`): Quiescent decaying turbulence.

Directional & Roughness Effects
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Wind Veer**: Directional rotation with height modeled via a power law: :math:`\theta(z) = \theta_{\text{ref}} + \Delta \theta \cdot (z/z_{\text{ref}})^{0.25}`.
* **Cross-Wind Bias**: Captures cross-wind shear asymmetries in complex topography.
* **Roughness-Dependent Scales**: Automatic scaling of integral length scales and intensities based on aerodynamic roughness length :math:`z_0` (Smooth Water, Grassland, Shrubland, Forest, Urban).

----

IEC 61400-1 Standard (Normal Turbulence Model)
----------------------------------------------

The solver implements the industry-standard IEC 61400-1:2019 wind turbine design requirements for synthetic fluctuation and time-series generation.

Atmospheric Anisotropy
~~~~~~~~~~~~~~~~~~~~~~

The target Root Mean Square (RMS) velocities are scaled from the longitudinal turbulence intensity profile :math:`I(z)` following standard boundary layer ratios:

.. math::

   u_{\text{rms}} = I(z) \cdot U_{\text{mean}}, \qquad v_{\text{rms}} = 0.8 \cdot u_{\text{rms}}, \qquad w_{\text{rms}} = 0.5 \cdot u_{\text{rms}}

Spectral Formulations
~~~~~~~~~~~~~~~~~~~~~

The user can choose between two standard spectral density models:

* **Von Kármán Isotropic Spectrum**:
  
  .. math::
  
     S_u(f) = \frac{4 \cdot L_u \cdot u_{\text{rms}}^2}{\left[1 + 70.8 \cdot \left(f \cdot L_u / U_{\text{mean}}\right)^2\right]^{5/6}}

* **Kaimal Empirical Spectrum**:
  
  .. math::
  
     S_u(f) = \frac{4 \cdot L_u \cdot u_{\text{rms}}^2 \cdot \hat{f}}{\left(1 + 6 \cdot \hat{f}\right)^{5/3}}

  where :math:`\hat{f} = f \cdot L_u / U_{\text{mean}}` is the normalized frequency.

Monin-Obukhov Stability Corrections
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Non-neutral thermal stratification adjusts both the turbulence intensity profile and the integral length scales:

* **Momentum Stability Functions** (:math:`\psi_m`):
  * **Stable** (:math:`\zeta = z/L > 0`): :math:`\psi_m(\zeta) = -5\zeta` (Businger-Dyer) or Holtslag-De Bruin parameterization for strong inversions.
  * **Unstable** (:math:`\zeta < 0`): :math:`\psi_m(\zeta) = 2\ln((1+x)/2) + \ln((1+x^2)/2) - 2\arctan(x) + \pi/2` with :math:`x = (1 - 16\zeta)^{1/4}`.

* **Turbulence Intensity Modification**: :math:`I(z) = I_{\text{neutral}}(z) \times f(\zeta)`
  * **Stable**: :math:`f(\zeta) = 1/\sqrt{1 + 5\zeta}` (mixing suppressed).
  * **Unstable**: :math:`f(\zeta) = (1 - 16\zeta)^{1/4}` (buoyancy-enhanced mixing).

* **Length Scale Modification**: :math:`L_u(z) = L_{u,\text{neutral}} \times g(\zeta)`
  * **Stable**: :math:`g(\zeta) = 1/(1 + 3\zeta)`.
  * **Unstable**: :math:`g(\zeta) = (1 - 16\zeta)^{1/8}`.

Coherence and Decay Models
~~~~~~~~~~~~~~~~~~~~~~~~~~

The solver supports advanced decay models to account for spatial and temporal decorrelation:
* **QuadraticExponential**: Smooth quadratic exponential spatial decay.
* **PowerLaw**: Algebraic spatial decay of coherence.
* **SmoothProfile**: Smooth height-varying profile transition.

----

Validation & Diagnostics Framework
----------------------------------

To ensure physical and numerical correctness, the solver features a validation suite that computes and exports diagnostic statistics:

* **Spectral Power Density (PSD)**: Computed via Fourier transform of the generated time series.
* **Energy Balance**: Verifies that the integrated spectral density matches the target variance, :math:`\int S(f)df = \sigma^2`.
* **Integral Length Scale**: Verified from the low-frequency limit of the PSD:
  
  .. math::
  
     L_i = \frac{U_{\text{mean}}}{\pi} \cdot \int \frac{S_i(f)}{U_{\text{mean}}^2} df

* **Turbulence Statistics**: Analyzes statistical moments of the synthetic wind fields:
  * **Friction Velocity** (:math:`u_*`): Computed from surface shear stress.
  * **Skewness**: Quantifies asymmetry of fluctuations (target 0 for Gaussian).
  * **Kurtosis**: Quantifies tail heaviness (target 3 for Gaussian).
  * **Correlation Coefficients**: Verifies off-diagonal correlations.

----

.. toctree::
   :maxdepth: 2
   :caption: Feature Guides & Manuals:

   MANN_BOX_USER_GUIDE
   MANN_BOX_API_REFERENCE
   MANN_BOX_BEST_PRACTICES
   IEC61400_FLUCTUATION_GENERATION
   MANN_BOX_PYTHON_BINDINGS

.. toctree::
   :maxdepth: 2
   :caption: Technical Summaries & Analyses:

   IMPLEMENTATION_SUMMARY
   IMPLEMENTATION_NOTES
   TERRAIN_AWARE_FLUCTUATIONS
   TURBULENCE_MODELS_ANALYSIS
   MANN_IEC_COMPLEX_TERRAIN_ANALYSIS
   MANN_BOX_TEST_CASE

