.. _iec61400_synthesis:

IEC 61400-1 Fluctuation Synthesis
=================================

This section details the physical models, numerical formulations, and validated capabilities of the standard **IEC 61400-1** continuous normal turbulence fluctuation synthesis in the mass-consistent wind solver.

----

Core Formulation
----------------

The solver implements the industry-standard IEC 61400-1:2019 wind turbine design requirements for synthetic fluctuation and time-series generation.

Atmospheric Anisotropy
~~~~~~~~~~~~~~~~~~~~~~

The target Root Mean Square (RMS) velocities are scaled from the longitudinal turbulence intensity profile :math:`I(z)` following standard boundary layer ratios:

.. math::

   u_{\text{rms}} = I(z) \cdot U_{\text{mean}}, \qquad v_{\text{rms}} = 0.8 \cdot u_{\text{rms}}, \qquad w_{\text{rms}} = 0.5 \cdot u_{\text{rms}}

Spectral Models
~~~~~~~~~~~~~~~

The user can choose between two standard spectral density models:

* **Von Kármán Isotropic Spectrum**:
  
  .. math::
  
     S_u(f) = \frac{4 \cdot L_u \cdot u_{\text{rms}}^2}{\left[1 + 70.8 \cdot \left(f \cdot L_u / U_{\text{mean}}\right)^2\right]^{5/6}}

* **Kaimal Empirical Spectrum**:
  
  .. math::
  
     S_u(f) = \frac{4 \cdot L_u \cdot u_{\text{rms}}^2 \cdot \hat{f}}{\left(1 + 6 \cdot \hat{f}\right)^{5/3}}

  where :math:`\hat{f} = f \cdot L_u / U_{\text{mean}}` is the normalized frequency.

Monin-Obukhov Stability Corrections
-----------------------------------

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
--------------------------

The solver supports advanced decay models to account for spatial and temporal decorrelation:
* **QuadraticExponential**: Smooth quadratic exponential spatial decay.
* **PowerLaw**: Algebraic spatial decay of coherence.
* **SmoothProfile**: Smooth height-varying profile transition.

----

.. toctree::
   :maxdepth: 2
   :caption: IEC 61400-1 Guides & Analyses:

   IEC61400_FLUCTUATION_GENERATION
   iec61400
