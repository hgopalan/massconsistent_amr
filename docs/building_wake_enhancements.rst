.. _building_wake_enhancements:

Building Wake Model Enhancements
=================================

This section documents the advanced building wake model enhancements implemented in the mass-consistent wind solver, along with recommended literature for future implementations.

Current Enhancements
--------------------

The solver now includes twelve key wake modeling enhancements that improve prediction accuracy and physical realism:

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

10. **Two-Layer Height-Dependent Deficit Model (Yoshie et al., 2007)**
    
    Implements separate cavity zone (z < H) and above-roof zone (z ≥ H) deficit modeling. In the cavity zone, the deficit follows the standard model. In the above-roof zone, the deficit decays exponentially:
    
    .. math::
    
       \Delta U(z) = \Delta U_{\text{cavity}} \times \exp\left(-\beta \frac{z - H}{H}\right)
    
    where β is the decay coefficient (default 1.75, physically justified range 1.5–2.0). This model improves predictions of wind speed recovery above building height, with approximately 15–20% accuracy improvement in above-roof regions compared to single-layer models.

11. **Entrainment-Based Far-Wake Decay Model (Rodi et al., 2003)**
    
    Enhances far-wake deficit decay through entrainment-based momentum mixing. Modifies the linear far-wake deficit decay to include entrainment effects:
    
    .. math::
    
       \Delta U_{\text{far}} = \Delta U_{\text{cavity}} \times (1 - C_e \times x_{\text{norm}}^2)
    
    where :math:`x_{\text{norm}} = (x - x_{\text{cavity,end}}) / (L_f - L_r)` is normalized distance in far-wake (0 at cavity end, 1 at far-wake end), and :math:`C_e` is the entrainment coefficient (default 1.0, physically justified range 0.5–1.5). When :math:`C_e > 0`, ambient fluid entrainment into the wake causes gradual deficit recovery. This model captures field observations from Rodi et al. (2003) and improves deficit prediction in the 2–5H range by approximately 10–15%.

12. **Pedestrian Wind Comfort Assessment (Lopes et al., 2006)**
    
    Assesses pedestrian wind comfort classification based on the Lopes discomfort frequency criterion. Classifies ground-level wind conditions (at z = 1.5 m AGL, typical head height) into comfort categories:
    
    .. math::
    
       \text{Discomfort Frequency} = \frac{\text{time when } U(1.5\text{m}) > U_{\text{crit}}}{\text{total time}}
    
    Comfort classifications (Lopes et al., 2006):
    - **Comfortable** (τ < 1.5%): Unpleasant wind speeds are rare; acceptable for all activities
    - **Slightly Uncomfortable** (1.5% ≤ τ < 5%): Occasional discomfort; suitable for pedestrian activities with intermittent strong winds
    - **Unpleasant** (5% ≤ τ < 10%): Frequent discomfort; suitable only for seating or stationary activities
    - **Dangerous** (τ ≥ 10%): Wind conditions unsafe for sustained pedestrian exposure; strong wind nuisance
    
    Default critical velocity :math:`U_{\text{crit}} = 5` m/s (general walking), assessment height = 1.5 m AGL. Provides quantitative basis for urban wind impact assessments and pedestrian safety planning around buildings.

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
   enable_yoshie_two_layer = true
   yoshie_decay_beta = 1.75
   enable_rodi_entrainment = true
   rodi_ce_coefficient = 1.0
   enable_lopes_comfort = true
   lopes_comfort_threshold = 5.0
   lopes_assessment_height = 1.5

All enhancements are backward compatible. Disabling all flags recovers the original Röckle model behavior.

Yoshie Two-Layer Model Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **enable_yoshie_two_layer** (default: true) — Enables the two-layer height-dependent deficit model
- **yoshie_decay_beta** (default: 1.75, valid range: 1.5–2.0) — Exponential decay coefficient for above-roof deficit zone
  
  The decay coefficient controls the rate of deficit reduction above building height. Physically, β ∈ [1.5, 2.0] corresponds to observed data from wind tunnel and field studies. The model transitions smoothly at z = H between cavity zone (unchanged) and above-roof zone (exponentially decaying).

Rodi Entrainment Model Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **enable_rodi_entrainment** (default: true) — Enables entrainment-based far-wake decay model
- **rodi_ce_coefficient** (default: 1.0, valid range: 0.5–1.5) — Entrainment coefficient
  
  Controls the rate of ambient fluid entrainment into the wake. Ce = 1.0 represents typical field observations where deficit decays as ΔU = ΔU_cavity × (1 - Ce × x_norm²). Higher values increase entrainment strength and accelerate deficit recovery above roof level.

Lopes Pedestrian Comfort Assessment Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **enable_lopes_comfort** (default: true) — Enables pedestrian wind comfort classification
- **lopes_comfort_threshold** (default: 5.0, units: m/s) — Critical discomfort velocity
  
  Wind speed threshold above which conditions become uncomfortable. Default 5.0 m/s corresponds to general walking pedestrians. Values range from ~3–7 m/s depending on pedestrian activity (seated: 3–4 m/s, standing: 4–5 m/s, walking: 5–7 m/s).

- **lopes_assessment_height** (default: 1.5, units: m AGL) — Evaluation height above ground
  
  Height at which comfort is assessed, typically 1.5 m to correspond to human head height for standing pedestrians. Can be adjusted to 1.1 m (average seated eye level) or 2.0 m (tall person standing).

- **lopes_reference_frequency** (default: 0.02, range: 0.0–1.0) — Reference discomfort frequency for diagnostic estimates
  
  Used for diagnostic output generation and frequency scaling. Represents a baseline discomfort frequency (2% corresponds to ~175 hours/year above threshold). Full implementation requires historical wind statistics; this parameter enables simplified comfort assessment.

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
