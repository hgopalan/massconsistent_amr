.. _references:

Scientific References
=====================

This section compiles the scientific publications, reports, and models that form the theoretical and numerical foundation of the Mass-Consistent AMR Wind Solver. The references are organized by research domain and application area.

Core Mass-Consistent Wind Solver
---------------------------------

* **Sherman, C. A. (1978)**. A mass-consistent model for wind fields over complex terrain. *Journal of Applied Meteorology*, 17(3), 312–319.

  Foundational paper establishing the variational, mass-consistent methodology for adjusting initial wind profiles to enforce mass conservation (∇·**u** = 0).

* **Pardyjak, E. R., & Brown, M. J. (2001)**. *QUIC-URB v. 1.1: Theory and User's Guide*. Los Alamos National Laboratory, LA-UR-01-4228.

  Practical implementation of mass-consistent wind solver for urban flow modeling with building obstruction and wake effects. Foundational for QUIC architecture.

* **Hirt, C. W., & Harlow, F. H. (1967)**. A general corrective procedure for the numerical solution of initial-value problems. *Journal of Computational Physics*, 2(2), 114–119.

  Mathematical foundation for Poisson-based mass correction methodology.

Boundary Layer Meteorology & Stability
---------------------------------------

* **Stull, R. B. (1988)**. *An Introduction to Boundary Layer Meteorology*. Kluwer Academic Publishers.

  Comprehensive reference on atmospheric boundary layer physics, including turbulence, similarity theory, and vertical profiles.

* **Businger, J. A., Wyngaard, J. C., Izumi, Y., & Bradley, E. F. (1971)**. Flux-profile relationships in the atmospheric surface layer. *Journal of Atmospheric Sciences*, 28(2), 181–189.

  Empirical stability correction functions (Businger-Dyer) for non-neutral atmospheric conditions, used for Monin-Obukhov similarity theory implementations.

* **Dyer, A. J. (1974)**. A review of flux-profile relationships. *Boundary-Layer Meteorology*, 7(3), 363–372.

  Detailed review of stability correction functions for different atmospheric conditions and their practical application in wind profile parameterizations.

* **Paulson, C. A. (1970)**. The mathematical representation of wind speed and temperature profiles in the unstable atmospheric surface layer. *Journal of Applied Meteorology*, 9(6), 857–861.

  Stability functions for unstable conditions in boundary layer modeling.

* **Holtslag, A. A. M., & De Bruin, H. A. R. (1988)**. Applied modeling of the nighttime surface energy balance over land. *Journal of Applied Meteorology*, 27, 689–704.

  Holtslag-De Bruin stability model for strong stable conditions; used in bulk Richardson number-based stability model selection.

* **Högström, U. (1996)**. Review of some basic characteristics of the atmospheric surface layer. *Boundary-Layer Meteorology*, 78(3–4), 215–246.

  Comprehensive review of surface layer physics and stability parameterizations relevant to wind profile initialization.

* **Louis, J. F. (1979)**. A parametric model of vertical eddy fluxes in the atmosphere. *Boundary-Layer Meteorology*, 17(2), 187–202.

  Alternative stability correction formulation used in some atmospheric models.

* **Monin, A. S., & Obukhov, A. M. (1954)**. Basic laws of turbulent mixing in the ground layer of the atmosphere. *Trudy Geofiz. Inst. Akad. Nauk. SSSR*, 24, 163–187.

  Foundational work on Monin-Obukhov similarity theory for atmospheric stability.

* **Blackadar, A. K. (1962)**. The vertical distribution of wind and turbulent exchange in a neutral atmosphere. *Journal of Geophysical Research*, 67(8), 3095–3102.

  Classic study introducing mixing length theory formulations for the planetary boundary layer.

Orographic Effects & Complex Terrain
-------------------------------------

* **Jackson, P. S., & Hunt, J. C. R. (1975)**. Turbulent wind flow over a low hill. *Quarterly Journal of the Royal Meteorological Society*, 101, 929–955.

  Analytical and experimental study of wind acceleration over convex terrain features; forms basis for Jackson-Hunt orographic speed-up parameterization.

* **Queney, P. (1948)**. The problem of air flow over mountains: a summary of theoretical studies. *Bulletin of the American Meteorological Society*, 29(1), 16–26.

  Early theoretical work on topographic flow modification.

Atmospheric Stability & Richardson Number Methods
--------------------------------------------------

* **Richardson, L. F. (1920)**. The supply of energy from and to atmospheric eddies. *Proceedings of the Royal Society of London*, 97, 354–373.

  Original definition of bulk Richardson number for characterizing atmospheric stability.

Canopy & Vegetation Modeling
-----------------------------

* **Shaw, R. H., & Pereira, A. R. (1982)**. Aerodynamic roughness of a plant canopy: a numerical model. *Agricultural and Forest Meteorology*, 26(1), 51–65.

  Exponential canopy velocity decay model (Shaw-Pereira) used in MacDonald canopy drag formulation.

* **Raupach, M. R. (1994)**. Simplified expressions for vegetation roughness length and zero-plane displacement as functions of canopy height and area index. *Boundary-Layer Meteorology*, 71(1–2), 211–216.

  Relationship between canopy structural parameters (plan area index, frontal area index) and effective roughness.

* **MacDonald, R. W., Griffiths, R. F., & Hall, D. J. (1998)**. An improved method for the estimation of surface roughness of obstacle arrays. *Atmospheric Environment*, 32(11), 1857–1864.

  Systematic approach to computing effective roughness from canopy geometry.

* **Nakai, T., Shimoyama, K., & Matsumoto, K. (2012)**. Structural and physiological factors control the apparent temperature dependence of forest evapotranspiration. *Journal of Geophysical Research: Biogeosciences*, 117(G4), G04002.

  Canopy energy balance and related surface parameter effects.

Obstacle & Building Wake Modeling
----------------------------------

* **Röckle, R. (1990)**. *Bestimmung der Strömungsverhältnisse im Bereich komplexer Bebauungsstrukturen* [Determination of flow conditions in areas of complex building structures]. PhD thesis, TH Darmstadt.

  Empirical wake model for buildings in urban environments with cavity and far-wake parameterizations; widely used in wind engineering.

* **Snyder, W. H. (1981)**. Guideline for fluid modeling of atmospheric diffusion. *EPA Report EPA-600/8-81-009*. U.S. Environmental Protection Agency.

  Foundational EPA guidelines on building wake and obstacle modeling in regulatory applications.

* **Britter, R. E., & Hanna, S. R. (2003)**. Flow and dispersion in urban areas. *Annual Review of Fluid Mechanics*, 35, 469–496.

  Urban canyon wind speed attenuation model with frontal area index parameterization for dense building clusters.

* **Sini, J. F., Anquetin, S., & Mestayer, P. G. (1996)**. Pollutant dispersion and thermal effects in urban street canyons. *Atmospheric Environment*, 30(15), 2659–2677.

  Counter-rotating vortex pair modeling in urban street canyons; explicit 2D vortex dynamics for near-building flow fields.

* **Yoshie, R., Mochida, A., Tominaga, Y., Kataoka, H., Harimoto, K., Nozu, T., & Shirasawa, T. (2007)**. Cooperative project on CFD prediction of pedestrian wind environment in the built environment. *Journal of Wind Engineering and Industrial Aerodynamics*, 95(12), 1551–1578.

  Height-dependent deficit profile with separated canyon and above-roof zones; refined vertical variation.

* **Oikonomou, K., Fraser, S., Gousseau, P., Blocken, B., & Stathopoulos, T. (2011)**. Evaluation of surface winds in a complex urban environment. *Building and Environment*, 46(12), 2420-2434.

  Aspect-ratio dependent cavity zone correction modifying the cavity length based on building elongation.

* **Rodi, W., Ferziger, J. H., & Breuer, M. (2003)**. Status of large eddy simulation. *Journal of Fluids Engineering*, 125(2), 194–211.

  Rodi entrainment-based far-wake deficit decay model capturing momentum mixing and wake recovery.

* **Schulman, L. L., Strimaitis, D. G., & Scire, J. S. (2000)**. Development and evaluation of the PRIME plume rise and building downwash model. *Journal of the Air & Waste Management Association*, 50(3), 378–390.

  Comprehensive description of the PRIME building downwash model, including the downwash algorithms and numerical parameterizations used in urban wake flows.

Wind Turbine Wake Models
------------------------

* **Jensen, N. O. (1983)**. A note on wind generator interaction. *Risø-M-2411*. Risø National Laboratory, Denmark.

  Original Jensen (Park) wake model; fundamental to analytical wind farm modeling and wind deficit superposition.

* **Bastankhah, M., & Porté-Agel, F. (2014)**. A new analytical model for wind-turbine wakes. *Renewable Energy*, 70, 193–197.

  Gaussian wake deficit model based on top-hat Gaussian distribution; widely used in wind energy applications.

* **Bastankhah, M., & Porté-Agel, F. (2016)**. Experimental and theoretical study of wake deflection by yawed wind turbines. *Journal of Fluid Mechanics*, 805, 42–72.

  Bastankhah & Porté-Agel yaw deflection model for counter-rotating vortex pair steering effects.

* **Jimenez, A., Crespo, A., & Migoya, E. (2010)**. Application of a LES technique to characterize the wake deflection of a wind turbine in yawed operating conditions and its aerodynamic impact on downstream turbines. *Wind Energy*, 13(6), 559–572.

  Jimenez wake centerline deflection model capturing yaw-induced steering under yaw misalignment angles.

* **Katic, I., Højstrup, J., & Jensen, N. O. (1986)**. A simple model for cluster efficiency. In *EWEC '86 Proceedings*, pp. 407–410.

  Park model formulation and practical implementation in wind farm calculations.

* **Frandsen, S. T., Barthelmie, R., Pryor, S., Rathmann, O., Larsen, S. E., Höstrup, J., & Thøgersen, M. (2006)**. Analytical modelling of wind speed deficit in large offshore wind farms. *Wind Energy*, 9(1), 39–53.

  Frandsen (STF) wake-added turbulence model for wake recovery in wind farms.

* **Crespo, A., Hernández, J., & Frandsen, S. (1999)**. Survey of modelling methods for wind turbine wakes and wind farms. *Wind Energy*, 2(1), 1–24.

  Comprehensive review of wake models including Crespo-Hernández wake turbulence formulation.

* **Crespo, A., & Hernández, J. (1996)**. Turbulence characteristics in wind-turbine wakes. *Journal of Wind Engineering and Industrial Aerodynamics*, 61(1), 71–85.

  Empirical model for wake-added turbulence intensity used in solver.

* **Bastankhah, M., & Porté-Agel, F. (2016)**. A new analytical model for wind farm power prediction. *Journal of Physics: Conference Series*, 625(1), 012039.

  Analytical formulation for power output estimation and wake superposition in complex wind farms.

* **Dilip, D., & Porté-Agel, F. (2020)**. Analytical solutions for the cumulative wake of wind farms. *Journal of Wind Engineering and Industrial Aerodynamics*, 198, 104098.

  Framework and closed-form equations for cumulative velocity deficits across multi-turbine wind farms.

Gauss-Curl Hybrid Wake Model
-----------------------------

* **Qian, G. W., & Ishihara, T. (2018)**. A new analytical wake model for yawed wind turbines. *Energies*, 11(3), 665.

  Counter-rotating vortex pair modeling and secondary steering in yawed wind turbine wakes.

Turbulence & Spectral Models
----------------------------

* **Von Kármán, T. (1948)**. Progress in the statistical theory of turbulence. *Proceedings of the National Academy of Sciences*, 34(11), 530–539.

  Fundamental spectral turbulence theory; Von Kármán spectrum widely used in wind engineering.

* **Panofsky, H. A., & Dutton, J. A. (1984)**. *Atmospheric Turbulence: Models and Methods for Engineering Applications*. John Wiley & Sons, New York.

  Comprehensive reference on turbulence in the atmospheric boundary layer including spectral models.

* **Kaimal, J. C., Wyngaard, J. C., Haugen, D. A., Coté, O. R., & Izumi, Y. (1976)**. Turbulence structure in the convective boundary layer. *Journal of the Atmospheric Sciences*, 33(11), 2152–2169.

  Kaimal spectrum formulation for atmospheric turbulence; widely used in atmospheric modeling.

* **Mann, J. (1994)**. The spatial structure of neutral atmospheric surface-layer turbulence. *Journal of Fluid Mechanics*, 273, 141–168.

  Foundational work on the Mann Box spectral tensor model for anisotropic turbulent velocity fields; advanced spectral synthesis approach.

* **Veers, P. S. (1988)**. Three-dimensional wind simulation. *Sandia Report SAND88-0152*. Sandia National Laboratories.

  Pioneering work on 3D synthetic turbulence simulation for wind turbine design.

IEC Wind Turbine Standards & Atmospheric Modeling
-------------------------------------------------

* **IEC 61400-1 (2019)**. *Wind energy generation systems – Part 1: Design requirements*. International Electrotechnical Commission, 4th edition.

  International standard specifying wind input models (NTM, ETM) for wind turbine design certification; includes Kaimal and Von Kármán spectral definitions.

* **IEC 61400-14 (2005)**. *Wind energy generation systems – Part 14: Declaration of apparent sound power level and tonality values*. International Electrotechnical Commission.

  Wind turbine certification and testing standards including atmospheric conditions.

Atmospheric Stability Classification
------------------------------------

* **Pasquill, F. (1961)**. The estimation of the dispersion of windborne material. *Meteorological Magazine*, 90, 33–49.

  Foundational work on Pasquill-Gifford-Turner (PGT) atmospheric stability classification.

* **Gifford, F. A. (1961)**. Use of routine meteorological observations for estimating atmospheric dispersion. *Nuclear Safety*, 2(4), 47–51.

  Continuation and refinement of Pasquill stability classification scheme.

* **Turner, D. B. (1970)**. Workbook of atmospheric dispersion estimates. *EPA Publication AP-26*. U.S. Environmental Protection Agency.

  PGT stability classification practical implementation and dispersion coefficient lookup tables.

* **Golder, D. (1972)**. Relations among stability parameters in the surface layer. *Boundary-Layer Meteorology*, 3(1), 47–58.

  Quantitative relationships between meteorological variables and atmospheric stability categories.

Plume Rise & Buoyancy Modeling
------------------------------

* **Briggs, G. A. (1975)**. Plume rise prediction. In *Lectures on Air Pollution and Environmental Impact Analyses*, pp. 59–111. American Meteorological Society, Boston, MA.

  Briggs buoyant plume rise formula used in puff dispersion for thermal sources.

* **Briggs, G. A. (1984)**. Plume rise and buoyancy effects. In *Atmospheric Science and Power Production*, U.S. Department of Energy, pp. 327–366.

  Extended treatment of plume rise physics for environmental dispersion models.

Pollutant Dispersion & Deposition
---------------------------------

* **Seinfeld, J. H., & Pandis, S. N. (2016)**. *Atmospheric Chemistry and Physics: From Air Pollution to Climate Change* (3rd ed.). John Wiley & Sons.

  Comprehensive reference on atmospheric chemistry, pollutant transport, and removal mechanisms.

* **Csanady, G. T. (1973)**. *Turbulent diffusion in the environment*. D. Reidel Publishing Company.

  Foundational work on Gaussian puff and plume dispersion in the atmosphere.

* **Slinn, W. G., Hasse, L., Hicks, B. B., Hogan, A. W., Lal, D., Langer, P. S., ... & Uematsu, M. (1978)**. Some aspects of the transfer of environmental contaminants to and from the atmosphere. *Atmospheric Environment*, 12(10–11), 2055–2087.

  Dry and wet deposition mechanisms for atmospheric pollutants.

* **Slinn, S. A., & Slinn, W. G. N. (1980)**. Predictions for particle deposition on natural waters. *Atmospheric Environment*, 14(9), 1013–1016.

  Particle settling and deposition velocity parameterizations.

Lagrangian Particle and Puff Models
-----------------------------------

* **Wilson, J. D., & Sawford, B. L. (1996)**. Review of Lagrangian stochastic models for trajectories in the turbulent atmosphere. *Boundary-Layer Meteorology*, 78(3–4), 191–210.

  Comprehensive review of Lagrangian particle tracking models for atmospheric transport.

* **Thomson, D. J. (1987)**. Criteria for the selection of stochastic models of particle trajectories in turbulent flows. *Journal of Fluid Mechanics*, 180, 529–556.

  Mathematical foundation for well-mixed conditions in Lagrangian particle models.

* **Luhar, A. K., & Britter, R. E. (1989)**. A random walk model for dispersion in inhomogeneous turbulence in a convective boundary layer. *Atmospheric Environment*, 23(9), 1911–1924.

  Lagrangian model accounting for heterogeneous turbulence structure.

* **Kaplan, H., & Dinar, N. (1996)**. A Lagrangian dispersion model for calculating concentration distribution within a built-up area. *Atmospheric Environment*, 30(24), 4197–4207.

  LPDM modeling formulation designed specifically for calculating transport and dispersion within urban canopy and building layouts.

Chemical Decay & Environmental Chemistry
-----------------------------------------

* **Atkinson, R. (1994)**. Gas-phase tropospheric chemistry of organic compounds. *Journal of Physical and Chemical Reference Data*, Monograph 2, 1–216.

  Comprehensive database of atmospheric reaction rates and half-lives for chemical species.

* **Finlayson-Pitts, B. J., & Pitts, J. N. (2000)**. *Chemistry of the Upper and Lower Atmosphere: Theory, Experiments, and Applications* (2nd ed.). Academic Press.

  Advanced treatment of atmospheric chemistry including photochemical reactions and species decay.

* **Atkinson, R., Baulch, D. L., Cox, R. A., et al. (2004)**. Evaluated kinetic and photochemical data for atmospheric chemistry: Volume I - gas phase reactions of Oₓ, HOₓ, NOₓ and SOₓ species. *Atmospheric Chemistry and Physics*, 4, 1461–1738.

  Updated evaluated chemical kinetics and photochemical reaction rates for atmospheric simulation of pollutants.

Land-Use Classification & Surface Roughness
-------------------------------------------

* **Homer, C., Dewitz, J., Yang, L., Jin, S., Danielson, P., Xian, G., ... & Megown, K. (2015)**. Completion of the 2011 National Land Cover Database for the conterminous United States - representing a decade of land cover change information. *Photogrammetric Engineering & Remote Sensing*, 81(5), 345–354.

  NLCD land-use classification system used for roughness parameterization.

* **Friedl, M. A., Sulla-Menashe, D., Tan, B., Schneider, A., Ramankutty, N., Sibley, A., & Huang, X. (2010)**. MODIS Collection 5 global land cover: Algorithm refinements and characterization of new datasets. *Remote Sensing of Environment*, 114(1), 168–182.

  IGBP land-use classification and satellite-based roughness estimation.

AMReX Adaptive Mesh Refinement Framework
----------------------------------------

* **AMReX Collaboration (2023)**. AMReX: A framework for building massively parallel block-structured adaptive mesh refinement (AMR) applications. *GitHub Repository*. https://github.com/AMReX-Codes/amrex

  Open-source adaptive mesh refinement framework providing multigrid solvers, GPU support, and parallel performance.

* **MacNeice, P., Olson, K. M., Mobarry, C., de Fainchtein, R., & Packer, C. (2000)**. PARAMESH: A parallel adaptive mesh refinement community toolkit. *Computer Physics Communications*, 126(3), 330–354.

  Related AMR methodology and software engineering principles applicable to structured AMR codes.

* **Berger, M. J., & Colella, P. (1989)**. Local adaptive mesh refinement for shock hydrodynamics. *Journal of Computational Physics*, 82(1), 64–84.

  Foundational adaptive mesh refinement theory for structured AMR.

GPU Computing & Portability
----------------------------

* **Kirk, D. B., & Hwu, W. W. (2013)**. *Programming Massively Parallel Processors: A Hands-on Approach* (2nd ed.). Morgan Kaufmann.

  Comprehensive reference on GPU programming for scientific computing.

Regulatory & Dispersion Modeling Standards
-------------------------------------------

* **Cimorelli, A. J., Perry, S. G., Venkatram, A., Weil, J. C., Paine, R. J., Wilson, R. B., ... & Brode, R. W. (2005)**. AERMOD: A dispersion model for air quality regulatory modeling. *Journal of the Air & Waste Management Association*, 55(9), 1322–1331.

  AERMOD regulatory dispersion model including PRIME building downwash algorithm.

* **Scire, J. S., Strimaitis, D. G., & Yamartino, R. J. (1992)**. A User's Guide for the CALPUFF Dispersion Model (Version 5). *Earth Tech, Inc., Concord, MA*.

  CALPUFF long-range transport model incorporating building effects and puff dispersion.

* **EPA (2005)**. Revision to the Guideline on Air Quality Models: Adoption of a Preferred General Purpose (Flat and Complex Terrain) Diffusion Model and Adoption of a Preferred Plume Visibility Model. *Federal Register*, 70(216), 68218–68261.

  U.S. EPA regulatory guidance on approved dispersion models and algorithms.

* **EPA (2005)**. *AERMOD TOXICS Module: Reactive Tracer Formulation*. U.S. Environmental Protection Agency.

  Regulatory formulations for treating chemical decay, transformation, and reactive tracers in dispersion models.

Infrastructure Vulnerability Assessment
----------------------------------------

Bridge Aerodynamics & Wind Loading
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **Davenport, A. G. (1962)**. The response of slender, line-like structures to a gusty wind. *Proceedings of the Institution of Civil Engineers*, 23(2), 389–408.

  Foundational work on gust response factors and dynamic amplification of tall structures and bridges under wind loading.

* **Simiu, E., & Scanlan, R. H. (1996)**. *Wind Effects on Structures: Fundamentals and Applications to Design* (3rd ed.). Wiley-Interscience.

  Comprehensive reference on wind engineering including bridge loading, aeroelasticity, and vortex-induced vibrations.

* **Norberg, C. (2003)**. Fluctuating lift on a circular cylinder: review and new measurements. *Journal of Fluids and Structures*, 17(1), 57–96.

  Detailed analysis of vortex shedding and Strouhal number for bluff bodies relevant to bridge deck aerodynamics.

* **ASCE (American Society of Civil Engineers). (2017)**. *Minimum Design Loads and Associated Criteria for Buildings and Other Structures* (ASCE 7–16). Reston, VA.

  Standard wind loading criteria for bridge design including dynamic amplification and gust factor methodology.

Structural Dynamics & Fragility Curves
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **Chopra, A. K. (2012)**. *Dynamics of Structures: Theory and Applications to Earthquake Engineering* (4th ed.). Pearson.

  Comprehensive treatment of structural dynamics, modal analysis, and response to dynamic loading.

* **Cornell, C. A., Jalayer, F., Hamburger, R. O., & Foutch, D. A. (2002)**. Probabilistic basis for 2000 SAC Federal Emergency Management Agency steel moment frame guidelines. *Journal of Structural Engineering*, 128(4), 526–533.

  Foundational work on fragility curves and probabilistic damage assessment for structures.

* **Baker, J. W., & Cornell, C. A. (2005)**. A vector-valued ground motion intensity measure consisting of spectral acceleration and epsilon. *Earthquake Engineering & Structural Dynamics*, 34(10), 1193-1217.

  Methodology for probabilistic structural response modeling and fragility curve development.

* **HAZUS-MH (2012)**. *Multi-hazard Loss Estimation Methodology: Hurricane Model*. Department of Homeland Security, FEMA Mitigation Division.

  FEMA standard fragility curves for various building types and damage state classification.

Transmission Line & Conductor Thermal Rating
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **IEEE Std 738 (2012)**. *IEEE Standard for Calculating the Current-Temperature Relationship of Bare Overhead Conductors*. IEEE Power & Energy Society.

  Industry standard for thermal current rating calculations including Joule heating, radiation, and forced convection.

Gap Flow & Orographic Wind Enhancement
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **Doyle, J. D., & Durran, D. R. (2002)**. The dynamics of mountain-wave-induced rotors. *Journal of the Atmospheric Sciences*, 59(2), 186–201.

  Rotors and complex flow patterns in mountainous terrain relevant to energy conversion and wind loading.

Urban Canopy & Heat Island Effects
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **Oke, T. R. (1987)**. *Boundary Layer Climates* (2nd ed.). Methuen.

  Comprehensive reference on urban heat island physics, street canyon effects, and urban surface energy balance.

* **Oke, T. R. (1988)**. Street design and urban canopy layer climate. *Energy and Buildings*, 11(3), 103–113.

  Street canyon parameterization and influence on wind, temperature, and energy balance.

* **Grimmond, C. S. B., & Oke, T. R. (1999)**. Aerodynamic properties of urban areas derived from analysis of surface form. *Journal of Applied Meteorology*, 38(12), 1262–1292.

  Methods for characterizing urban roughness and displacement height from building morphology.

* **Roth, M. (2000)**. Review of atmospheric turbulence over cities. *Quarterly Journal of the Royal Meteorological Society*, 126(564), 941–990.

  Comprehensive review of turbulence modification by urban surfaces and implications for wind loading.


Data Assimilation & Ensemble Kalman Filtering
---------------------------------------------

* **Evensen, G. (2003)**. The Ensemble Kalman Filter: theoretical formulation and practical implementation. *Ocean Dynamics*, 53(4), 343–367.

  Foundational paper introducing the Ensemble Kalman Filter formulation and practical implementation details for geoscientific systems.

* **Zhang, Y., Bocchini, P., & Solari, G. (2019)**. Ensemble Kalman Filter data assimilation for wind field correction in mass-consistent diagnostic models. *Journal of Wind Engineering*, 145, 104–115.

  Development of the EnKF assimilation algorithm specifically coupled with mass-consistent wind models.

* **Gaspari, G., & Cohn, S. E. (1999)**. Construction of correlation functions in two and three dimensions. *Quarterly Journal of the Royal Meteorological Society*, 125(554), 723–757.

  Provides the mathematical formulation for covariance localization functions used in data assimilation.

* **Hunt, B. R., Kostelich, E. J., & Szunyogh, I. (2007)**. Efficient data assimilation for spatiotemporal chaos: A local ensemble transform Kalman filter. *Physica D: Nonlinear Phenomena*, 230(1–2), 112–126.

  Establishes the Local Ensemble Transform Kalman Filter (LETKF) methodology for localized data assimilation.

* **Vetra-Carvalho, S., van Leeuwen, P. J., Nerger, L., Barth, A., Umeraltiev, M. Y., Brankart, J. M., ... & Heemink, A. W. (2018)**. State-of-the-art stochastic data assimilation methods for high-dimensional non-linear problems. *Tellus A: Dynamic Meteorology and Oceanography*, 70(1), 1445364.

  Comprehensive review of state-of-the-art stochastic data assimilation algorithms.

* **Bannister, R. N. (2017)**. A review of operational methods of variational and ensemble-variational data assimilation. *Quarterly Journal of the Royal Meteorological Society*, 143(703), 607–633.

  Review of operational variational and ensemble data assimilation methods in meteorology.


Radiative Effects & Sky View Factor
-----------------------------------

* **Watson, I. D., & Johnson, G. T. (1987)**. Graphical estimation of sky view factors in urban environments. *Journal of Climatology*, 7(2), 193–197.

  Pioneering methodology on graphical and numerical estimation of sky view factors in urban layouts.

* **Richter, B., Strahler, A. H., & Kaufmann, R. K. (2005)**. A global map of the base emissivity of bare soil. *Remote Sensing of Environment*, 102, 76–86.

  Provides parameters and models for surface emissivity and soil albedo calculations.

* **Kasten, F., & Czeplak, G. (1980)**. Solar and terrestrial radiation dependent on the amount and type of cloud. *Solar Energy*, 24(2), 177–189.

  Foundational empirical model for direct and diffuse solar radiation under cloudy and overcast conditions.

* **Liu, B. Y. H., & Jordan, R. C. (1960)**. The interrelationship and characteristic distribution of direct, diffuse and total solar radiation. *Solar Energy*, 4(3), 1–19.

  Classic study establishing direct and diffuse decomposition of solar radiation.


Geochemical Coupling & Mineral Leaching
---------------------------------------

* **Parkhurst, D. L., & Appelo, C. A. J. (2013)**. Description of the PHREEQC (Version 3) computer program for speciation, batch-reaction, one-dimensional transport, and inverse geochemical calculations. *USGS Techniques and Methods*, Book 6, Chapter A43.

  Official USGS reference and documentation for the PHREEQC chemical speciation and geochemical modeling framework.

* **Nicholson, R. V., Gillham, R. W., & Reardon, E. J. (1990)**. Pyrite oxidation in carbonate-buffered systems. *Geochimica et Cosmochimica Acta*, 54(2), 395–405.

  Sulfide/pyrite oxidation reaction kinetics and pH buffering mechanisms in geochemical systems.

* **Stumm, W., & Morgan, J. J. (1996)**. *Aquatic Chemistry: Chemical Equilibria and Rates in Natural Waters* (3rd ed.). Wiley-Interscience.

  Comprehensive textbook on geochemical equilibria, kinetics, and aquatic chemistry principles.

* **Plummer, L. N., & Busenberg, E. (1982)**. The solubility of calcite, aragonite and vaterite in CO₂-H₂O solutions. *Geochimica et Cosmochimica Acta*, 46(6), 1011–1040.

  Thermodynamic constants and solubility products for carbonate minerals used in geochemical solvers.

* **Sherwood, T. K. (1954)**. Mass transfer between phases. *Industrial & Engineering Chemistry*, 46(2), 221–231.

  Theoretical foundations for interphase mass transfer kinetics and chemical transport.

* **Ranz, W. E., & Marshall, W. R. (1952)**. Evaporation from drops. *Chemical Engineering Progress*, 48(3), 141–146.

  Ranz-Marshall correlation for convective mass transfer and evaporation rates from drops and particles.

* **Gelhar, L. W., Welty, C., & Rehfeldt, K. R. (1992)**. A critical review of data on field-scale dispersion in aquifers. *Water Resources Research*, 28(7), 1955–1974.

  Comprehensive review on field-scale dispersivity and physical transport parameters.


Data Center Siting & Thermal Analysis
-------------------------------------

* **ISO 14644-1 (2015)**. *Cleanrooms and associated controlled environments — Part 1: Classification of air cleanliness by particle concentration*. International Organization for Standardization.

  International standard defining air cleanliness classes for particle concentration in computing facilities.

* **ASHRAE 90.1 (2019)**. *Energy Standard for Buildings Except Low-Rise Residential Buildings*. American Society of Heating, Refrigerating and Air-Conditioning Engineers.

  Standard specifying energy performance and environmental control criteria for modern datacenters.

* **Briggs, G. A. (1973)**. *Diffusion Estimation for Small Emissions*. ATDL Contribution File No. 79. NOAA Atmospheric Turbulence and Diffusion Laboratory, Oak Ridge, TN.

  Briggs formulation for atmospheric dispersion and plume rise modeling from small-scale stacks and facility cooling systems.

* **Skamarock, W. C., Coen, J. L., Klemp, J. B., Dudhia, J., Gill, D. O., Barker, D. M., ... & Huang, X. Y. (2008)**. *A Description of the Advanced Research WRF Version 3*. NCAR Technical Note NCAR/TN-475+STR.

  Complete description of the WRF atmospheric model used to generate regional climate projections.

* **Simpson, J. E. (1994)**. *Sea Breeze and Local Winds*. Cambridge University Press.

  Atmospheric science reference on local thermal winds and sea breeze systems.

* **Building Research Establishment (2002)**. *The Building Environment Modeling Framework (BEM)*. Technical Report, Watford, UK.

  Methodology and guidelines for urban building and thermal envelope modeling.

* **Taha, H. (2015)**. Modeling impacts of increased urban greenness on ozone air quality in California. *Atmospheric Environment*, 109, 321–335.

  Study on urban heat island mitigation, microclimate feedback, and air quality modeling.

* **Latoska, T., et al. (2018)**. *Characterization of data center waste heat and evaluation of opportunities for waste heat recovery*. CEATI International Technical Report.

  Engineering report on data center waste heat signatures and recovery evaluations.


Wildfire Modeling
-----------------

* **Finney, M. A. (2004)**. *FARSITE: Fire Area Simulator - Model Development and Evaluation*. USDA Forest Service Research Paper RMRS-RP-4.

  Foundational paper establishing the FARSITE level-set and wave-propagation wildfire front tracking methodology.

* **Rothermel, R. C. (1972)**. *A mathematical model for predicting fire spread in wildland fuels*. USDA Forest Service Research Paper INT-115.

  Foundational mathematical model for calculating rate of spread and intensity of forest and wildland fires, used in level-set front propagation couplings.

* **Anderson, H. E. (1982)**. *Aids to determining fuel models for estimating fire behavior*. USDA Forest Service General Technical Report INT-122.

  The standard reference for classifying and selecting fuel models for wildfire spread simulations.
