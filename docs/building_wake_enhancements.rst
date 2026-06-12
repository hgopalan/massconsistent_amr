.. _building_wake_enhancements:

Building Wake Model Enhancements
=================================

This section documents the advanced building wake model enhancements implemented in the mass-consistent wind solver, along with recommended literature for future implementations.

Current Enhancements
--------------------

The solver now includes nine key wake modeling enhancements that improve prediction accuracy and physical realism:

1. **Far-wake Extension to 15H**
   
   Extends far-wake zone influence from typical 3–5H to 15 building heights downstream, capturing long-range wake recovery effects.

2. **Oblique Angle Cavity Scaling**
   
   Scales cavity length based on wind approach angle: :math:`L_r(\theta) = L_r^0 \times \cos(\theta)`, where :math:`\theta` is the angle from building normal.

3. **Tall-Building Aspect-Ratio Correction**
   
   Applies aspect-ratio dependent correction: :math:`L_r = 0.9H \times \max(1.0, \min(W/H, 1.5))`, where :math:`H` is height and :math:`W` is width (crosswind).

4. **Gaussian Lateral Wake Profile**
   
   Optional Gaussian-profile deficit instead of linear profile: :math:`\Delta U \propto \exp(-(y/\sigma)^2)`, providing smoother lateral distribution.

5. **Upwind Recirculation Zone**
   
   Models reverse flow approximately :math:`0.5 \times \min(H,W)` upstream of building, capturing stagnation and flow diversion effects.

6. **Log-law Reference Velocity Correction**
   
   Extracts reference velocity from log-law profile: :math:`U(z) = U_{\text{ref}} \times \frac{\ln(z/z_0)}{\ln(z_{\text{ref}}/z_0)}` instead of local grid values.

7. **Corner and Side Acceleration**
   
   Adds velocity amplification at building corners and sides, modeling flow acceleration around sharp edges.

8. **Height-Dependent Velocity Variance Correction**
   
   Modifies velocity variance profile: reduced in cavity (0.5×), increased in shear layer (1.5×), based on height above ground.

9. **Horseshoe Vortex Modeling**
   
   Computes velocity perturbations from horseshoe vortex at building base, modeling circulation at the junction between building and ground.

Configuration Parameters
------------------------

Wake enhancements are controlled via input parameters in the AMReX inputs file:

.. code-block:: text

   enable_oblique_scaling = true
   enable_tall_building_correction = true
   enable_gaussian_profile = false
   enable_upwind_recirculation = true
   enable_reference_correction = false
   enable_corner_acceleration = true
   enable_variance_correction = false
   enable_horseshoe_vortex = true
   enable_extended_farwake = true

All enhancements are backward compatible. Disabling all flags recovers the original Röckle model behavior.

Recommended Literature for Future Enhancements
-----------------------------------------------

High-Priority Implementations (Simple, High Impact)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Rodi Entrainment Model (Rodi, 1986)**

Expression for continuous wake recovery:

.. math::

   \frac{dU}{dx} = -b \cdot U \cdot \frac{dH}{dx}

where :math:`b = 0.15-0.25` is the entrainment coefficient. This replaces simple linear decay with physically-grounded continuous entrainment.

**Yoshie Height-Dependent Deficit (Yoshie et al., 2007)**

Two-layer model for above-roof effects:

.. math::

   \frac{\Delta U}{U_{\text{ref}}}(z) = \begin{cases}
   \frac{\Delta U_{\text{canyon}}}{U_{\text{ref}}} & z < H \\
   \frac{\Delta U_{\text{canyon}}}{U_{\text{ref}}} \times \exp(-\beta(z-H)/H) & z \geq H
   \end{cases}

**Oikonomou Aspect-Ratio Refinement (Oikonomou et al., 2017)**

Improved aspect-ratio scaling:

.. math::

   U_{\text{exit}} = U_{\text{ref}} \times \left[0.2 + 0.8 \times (1 + H/W)^{-0.5}\right]

Medium-Priority Implementations (Moderate Complexity, Good Physics)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Jensen Power-Law Recovery (Jensen, 1979)**

Extended far-wake profile:

.. math::

   \Delta U(x, y) = U_{\text{ref}} \times c \times \left(\frac{H}{x}\right)^{\alpha} \times \exp\left(-\left(\frac{y}{y_w}\right)^2\right)

with :math:`c \approx 0.5`, :math:`\alpha \approx 0.5` for buildings.

**Blocken Separable Form (Blocken & Carmeliet, 2004)**

3D separable factorization:

.. math::

   \frac{\Delta U}{U_{\text{ref}}}(x, y, z) = A(x) \times f_{\text{lateral}}(y, W) \times f_{\text{vertical}}(z, H)

where:
- :math:`A(x) = c_1 \times \exp(-c_2 \times x/H)` with :math:`c_1 \approx 0.4`, :math:`c_2 \approx 1.5`
- :math:`f_{\text{lateral}}(y, W) = \exp(-(2y/W)^2)`
- :math:`f_{\text{vertical}}(z, H) = (1 + \sin(\pi z/H))^{c_3}` with :math:`c_3 \approx 1.0`

**Murakami Non-Dimensional Form (Murakami & Uehara, 1983)**

Self-similar scaling:

.. math::

   \frac{\Delta U^*}{U_{\text{ref}}} = \beta \times \left(\frac{H^*}{x^* + H^*}\right)^{\alpha}

with :math:`\alpha \approx 1.0`, :math:`\beta \approx 0.3`.

Lower-Priority Implementations (Advanced or Specialized)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Snyder-Lawson Downwash Angle (Snyder & Lawson, 1994)**

Vertical deflection modeling:

.. math::

   z_{\text{displaced}}(x, y) = \arctan(0.3 \times W/H) \times x \times \left[1 - (2y/W)^2\right] \times (H/x)^{0.5}

**Duenas Parametric Model (Duenas et al., 2006)**

Combined decay and spreading:

.. math::

   \frac{\Delta U}{U_{\text{ref}}} = (c_1 + c_2 \times x/H) \times \exp\left(-\left(\frac{y - y_{\text{offset}}}{\sigma}\right)^2\right)

with :math:`\sigma = \sigma_0 + c_3 \times x`.

**Solazzo Plume Rise (Solazzo & Britter, 2007)**

Thermal coupling (Phase 4+):

.. math::

   z_{\text{plume}} = z_{\text{source}} + (\Delta T/T_{\text{ref}})^{1/3} \times x

**Sini Counter-Rotating Vortex Pair (Sini et al., 1996)**

Explicit 2D vortex dynamics:

.. math::

   (u, v) = \pm \frac{\Gamma}{2\pi} \times \frac{[(x-x_c), -(y-y_c)]}{[(x-x_c)^2 + (y-y_c)^2]}

with :math:`\Gamma = 0.25 \times U_{\text{ref}} \times W \times (H/z)^{0.5}`.

References
----------

See :ref:`references` section for the complete bibliography. Key citations for building wake modeling include:

- Röckle (1990): Foundational urban canyon wake model
- Huber-Snyder (EPA): Empirical aspect-ratio dependent model
- Pardyjak & Brown (2001): QUIC-URB implementation guide
- Jensen (1979): Power-law wake recovery
- Rodi (1986): Entrainment-based wake modeling
- Blocken & Carmeliet (2004): Separable 3D deficit profiles
- Yoshie et al. (2007): Height-dependent canyon effects
- Oikonomou et al. (2017): Modern aspect-ratio refinements
- Murakami & Uehara (1983): Non-dimensional self-similar forms
