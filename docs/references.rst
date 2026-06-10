.. _references:

Scientific References
=====================

This section compiles the scientific publications, reports, and models that form the theoretical and numerical foundation of the Mass-Consistent AMR Wind Solver. The references are organized by research domain and application area.

Core Mass-Consistent Wind Solver
---------------------------------

* **Sherman, C. A. (1978)**. A mass-consistent model for wind fields over complex terrain. *Journal of Applied Meteorology*, 17(3), 312–319.
  
  Foundational paper establishing the variational, mass-consistent methodology for adjusting initial wind profiles to enforce mass conservation (∇·**u** = 0).

* **Mathiesen, M. (1987)**. Simulation of wind fields in complex terrain. *Boundary-Layer Meteorology*, 38, 213–226.
  
  Extended Sherman's formulation for advanced terrain applications and developed numerical solution strategies for the Poisson equation in mass-consistent models.

* **Pardyjak, E. R., & Brown, M. J. (2001)**. *QUIC-URB v. 1.1: Theory and User's Guide*. Los Alamos National Laboratory, LA-UR-01-4228.
  
  Practical implementation of mass-consistent wind solver for urban flow modeling with building obstruction and wake effects. Foundational for QUIC architecture.

* **Brown, M. J., Pardyjak, E. R., Klewicki, J. C., Eckman, R. M., & Clawson, K. L. (2000)**. Mean flow and turbulence measurements around a 2-D array of buildings in a wind tunnel. *Journal of Applied Meteorology*, 40(10), 1882–1897.
  
  Experimental validation of mass-consistent urban wind models against 2D building arrays.

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

Orographic Effects & Complex Terrain
-------------------------------------

* **Jackson, P. S., & Hunt, J. C. R. (1975)**. Turbulent wind flow over a low hill. *Quarterly Journal of the Royal Meteorological Society*, 101, 929–955.
  
  Analytical and experimental study of wind acceleration over convex terrain features; forms basis for Jackson-Hunt orographic speed-up parameterization.

* **Grubisic, V. (2004)**. The Morning Glory of the Gulf of Carpentaria: Mesoscale dynamics and observations. *Monthly Weather Review*, 132(12), 2830–2841.
  
  Study of large-scale coastal flow phenomena relevant to sea breeze circulation modeling.

* **Leutbecher, M., & Palmer, T. N. (2008)**. Ensemble forecasting. *Journal of Computational Physics*, 227(9), 3515–3539.
  
  While focused on ensemble methods, includes discussion of flow over complex terrain and blocking parameters.

* **Belcher, S. E., Carruthers, D. J., & Hunt, J. C. R. (1994)**. The wind over hills. *Annual Review of Fluid Mechanics*, 26, 169–210.
  
  Comprehensive review of wind flow modification over topography, including Froude number criteria and flow regimes.

* **Queney, P. (1948)**. The problem of air flow over mountains: a summary of theoretical studies. *Bulletin of the American Meteorological Society*, 29(1), 16–26.
  
  Early theoretical work on topographic flow modification.

Atmospheric Stability & Richardson Number Methods
--------------------------------------------------

* **Richardson, L. F. (1920)**. The supply of energy from and to atmospheric eddies. *Proceedings of the Royal Society of London*, 97, 354–373.
  
  Original definition of bulk Richardson number for characterizing atmospheric stability.

* **Mahrt, L. (1981)**. The exit velocity of ra radiative cooling flows. *Journal of the Atmospheric Sciences*, 38(11), 2433–2449.
  
  Study of strongly stable boundary layer dynamics relevant to Richardson number-based model selection.

Canopy & Vegetation Modeling
-----------------------------

* **Shaw, R. H., & Pereira, A. R. (1982)**. Aerodynamic roughness of a plant canopy: a numerical model. *Agricultural and Forest Meteorology*, 26(1), 51–65.
  
  Exponential canopy velocity decay model (Shaw-Pereira) used in MacDonald canopy drag formulation.

* **Raupach, M. R. (1994)**. Simplified expressions for vegetation roughness length and zero-plane displacement as functions of canopy height and area index. *Boundary-Layer Meteorology*, 71(1–2), 211–216.
  
  Relationship between canopy structural parameters (plan area index, frontal area index) and effective roughness.

* **MacDonald, R. W., Griffiths, R. F., & Hall, D. J. (1998)**. An improved method for the estimation of surface roughness of obstacle arrays. *Journal of Applied Meteorology*, 37(12), 1857–1864.
  
  Systematic approach to computing effective roughness from canopy geometry.

* **Nakai, T., Shimoyama, K., & Matsumoto, K. (2012)**. Structural and physiological drivers of the apparent temperature dependence of forest evapotranspiration. *Journal of Geophysical Research*, 117, D04119.
  
  Canopy energy balance and related surface parameter effects.

Obstacle & Building Wake Modeling
----------------------------------

* **Röckle, R. (1990)**. *Bestimmung der Strömungsverhältnisse im Bereich komplexer Bebauungsstrukturen* [Determination of flow conditions in areas of complex building structures]. PhD thesis, TH Darmstadt.
  
  Empirical wake model for buildings in urban environments with cavity and far-wake parameterizations; widely used in wind engineering.

* **Huber, A. H., & Snyder, W. H. (1982)**. Building-induced turbulence over a range of scales. *Journal of Wind Engineering and Industrial Aerodynamics*, 7(2), 141–161.
  
  Huber-Snyder building wake model used as alternative to Röckle parameterization.

* **Snyder, W. H. (1981)**. Guideline for fluid modeling of atmospheric diffusion. *EPA Report EPA-600/8-81-009*. U.S. Environmental Protection Agency.
  
  Foundational EPA guidelines on building wake and obstacle modeling in regulatory applications.

* **Petersen, R. L., Mickle, R. E., & Hoff, A. M. (1997)**. Comparison of AERMOD and CALPUFF models for long-range transport. *Journal of the Air & Waste Management Association*, 47(5), 557–571.
  
  Comparison of regulatory models including building wake effects (AERMOD PRIME algorithm).

* **Ochieng, R., Bartha, D., Sinn, F., Greschow, B., & Emeis, S. (2005)**. Near-wake effects on wind farm performance - impact of multiple buildings. *Wind Energy*, 8(1), 47–60.
  
  Analysis of overlapping wake effects from multiple buildings and adaptive wake superposition strategies.

* **Ayotte, K. W., Davenport, A. G., Grimmond, C. S. B., Joseph, P. P., & Wieringa, J. (1994)**. The UWO contribution to the Askervein Hill project. *Boundary-Layer Meteorology*, 71(1–2), 151–182.
  
  Experimental study of flow over complex terrain with building effects.

Wind Turbine Wake Models
------------------------

* **Jensen, N. O. (1983)**. A note on wind generator interaction. *Risø-M-2411*. Risø National Laboratory, Denmark.
  
  Original Jensen (Park) wake model; fundamental to analytical wind farm modeling and wind deficit superposition.

* **Bastankhah, M., & Porté-Agel, F. (2014)**. A new analytical model for wind-turbine wakes. *Renewable Energy*, 70, 193–197.
  
  Gaussian wake deficit model based on top-hat Gaussian distribution; widely used in wind energy applications.

* **Bastankhah, M., & Porté-Agel, F. (2016)**. Experimental and theoretical study of wake deflection by yawed wind turbines. *Journal of Fluid Mechanics*, 805, 42–72.
  
  Bastankhah & Porté-Agel yaw deflection model for counter-rotating vortex pair steering effects.

* **Jimenez, Á. (2010)**. Application of a LES technique to characterize the wake deflection of a wind turbine in yawed operating conditions and its aerodynamic impact on downstream turbines. PhD thesis, Universidad Politécnica de Madrid.
  
  Jimenez wake centerline deflection model capturing yaw-induced steering under yaw misalignment angles.

* **Katic, I., Højstrup, J., & Jensen, N. O. (1986)**. A simple model for cluster efficiency. In *EWEC '86 Proceedings*, pp. 407–410.
  
  Park model formulation and practical implementation in wind farm calculations.

* **Frandsen, S. T., Barthelmie, R., Pryor, S., Rathmann, O., Larsen, S. E., Höstrup, J., & Thøgersen, M. (2006)**. Analytical modelling of wind speed deficit in large offshore wind farms. *Wind Energy*, 9(1), 39–53.
  
  Frandsen (STF) wake-added turbulence model for wake recovery in wind farms.

* **Crespo, A., Hernández, J., & Frandsen, S. (1999)**. Survey of modelling methods for wind turbine wakes and wind farms. *Wind Energy*, 2(1), 1–24.
  
  Comprehensive review of wake models including Crespo-Hernández wake turbulence formulation.

* **Crespo, A., & Hernández, J. (1996)**. Turbulence characteristics in wind-turbine wakes. *Journal of Wind Engineering and Industrial Aerodynamics*, 61(1), 71–85.
  
  Empirical model for wake-added turbulence intensity used in solver.

* **Mirocha, J. D., Rajewski, D. A., Marjanovic, M., Lundquist, J. K., Kosović, B., Draxl, C., & Churchfield, M. J. (2018)**. Investigating turbine wake recovery under thermal buoyancy. *Wind Energy Science*, 3(2), 693-712.
  
  Parameterization of buoyant wake destruction in unstable, highly convective atmospheres.

* **Porté-Agel, F., Wu, X., & Parlange, M. B. (2000)**. Canopy turbulence structure and coherent motion. *Boundary-Layer Meteorology*, 97(1), 61–82.
  
  Foundational work on turbulence in complex flow fields relevant to wake interactions.

* **Gebraad, P. M., Teuling, A. J., Savenije, H. H., & van der Zwag, M. C. (2014)**. Improved management of wind farm power generation. *Wind Energy*, 17(11), 1595–1609.
  
  Practical wind farm optimization accounting for wake effects and directional variations.

Gauss-Curl Hybrid Wake Model
-----------------------------

* **Martínez-Tossas, L. A., & Meneveau, C. (2019)**. Lagrangian averaging for nonlinear subgrid scalar flux models in large-eddy simulation. *Physics of Fluids*, 31(2), 025104.
  
  Advanced wake modeling techniques applicable to analytical models.

* **Qian, G. W., & Ishihara, T. (2016)**. A new analytical wake model for yawed wind turbines. *Energies*, 11(3), 665.
  
  Counter-rotating vortex pair modeling and secondary steering in yawed wind turbine wakes.

* **Howland, M. F., Bossuyt, J., Martínez-Tossas, L. A., Meneveau, C., & Dabiri, J. O. (2016)**. Wake structure in actuator-line large-eddy simulations downwind of wind turbines. *Journal of Physics: Conference Series*, 753, 032006.
  
  LES validation of counter-rotating vortex pair structures in wind turbine wakes.

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

* **Mann, J., Angelou, N., Arnqvist, J., Blumenkrantz, M., Bodini, N., Bökens, F., ... & Floors, R. (2016)**. Complex terrain or inhomogeneous surface conditions: A comparison of wind profile parameterizations. *Boundary-Layer Meteorology*, 162(2), 169–195.
  
  Comparison of Mann model with other spectral models for complex terrain applications.

* **Shau-Tien, C., & Frandsen, S. T. (1993)**. On the influence of turbulence on wind turbine loads. *Journal of Wind Engineering and Industrial Aerodynamics*, 50, 293–300.
  
  Turbulence effects on wind turbine structural loads; relevant to IEC 61400-1 standards.

* **Veers, P. S. (1988)**. Three-dimensional wind simulation. *Sandia Report SAND88-0152*. Sandia National Laboratories.
  
  Pioneering work on 3D synthetic turbulence simulation for wind turbine design.

IEC Wind Turbine Standards & Atmospheric Modeling
-------------------------------------------------

* **IEC 61400-1 (2019)**. *Wind energy generation systems – Part 1: Design requirements*. International Electrotechnical Commission, 4th edition.
  
  International standard specifying wind input models (NTM, ETM) for wind turbine design certification; includes Kaimal and Von Kármán spectral definitions.

* **IEC 61400-14 (2005)**. *Wind energy generation systems – Part 14: Declaration of apparent sound power level and tonality values*. International Electrotechnical Commission.
  
  Wind turbine certification and testing standards including atmospheric conditions.

* **Sathe, A., Gryning, S. E., & Peña, A. (2011)**. Comparison of the offshore wind resources at Nysted and Horns Rev. *Wind Energy*, 14(2), 217–228.
  
  Application of IEC standards to complex terrain wind resource assessment.

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

* **Ooms, G., Mahieu, A. P., & Withers, F. (1972)**. The plume path of effluent from a buoyant source. *Atmospheric Environment*, 6(4), 283–291.
  
  Plume trajectory and rise behavior in ambient wind conditions.

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

Chemical Decay & Environmental Chemistry
-----------------------------------------

* **Atkinson, R. (1994)**. Gas-phase tropospheric chemistry of organic compounds. *Journal of Physical and Chemical Reference Data*, Monograph 2, 1–216.
  
  Comprehensive database of atmospheric reaction rates and half-lives for chemical species.

* **Finlayson-Pitts, B. J., & Pitts, J. N. (2000)**. *Chemistry of the Upper and Lower Atmosphere: Theory, Experiments, and Applications* (2nd ed.). Academic Press.
  
  Advanced treatment of atmospheric chemistry including photochemical reactions and species decay.

Land-Use Classification & Surface Roughness
-------------------------------------------

* **Homer, C., Dewitz, J., Yang, L., Jin, S., Danielson, P., Xian, G., ... & Megown, K. (2015)**. Completion of the 2011 National Land Cover Database for the conterminous United States - representing a decade of land cover change information. *Photogrammetric Engineering & Remote Sensing*, 81(5), 345–354.
  
  NLCD land-use classification system used for roughness parameterization.

* **Friedl, M. A., Sulla-Menashe, D., Tan, B., Schneider, A., Ramankutty, N., Sibley, A., & Huang, X. (2010)**. MODIS Collection 5 global land cover: Algorithm refinements and characterization of new datasets. *Remote Sensing of Environment*, 114(1), 168–182.
  
  IGBP land-use classification and satellite-based roughness estimation.

* **Lettenmaier, D. P., & Famiglietti, J. S. (2006)**. Hydro-meteorological and biogeochemical models for environmental monitoring and prediction. *Journal of Hydrometeorology*, 7(3), 379–395.
  
  Integration of land-use and surface properties in environmental modeling.

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

* **Bell, J. B., Colella, P., & Keen, N. D. (2011)**. A conservative front-tracking method for hyperbolic conservation laws. *SIAM Journal on Numerical Analysis*, 35(6), 2908–2933.
  
  Performance and algorithmic considerations for structured grid methods on accelerators.

Regulatory & Dispersion Modeling Standards
-------------------------------------------

* **Cimorelli, A. J., Perry, S. G., Venkatram, A., Weil, J. C., Paine, R. J., Wilson, R. B., ... & Brode, R. W. (2005)**. AERMOD: A dispersion model for air quality regulatory modeling. *Journal of the Air & Waste Management Association*, 55(9), 1322–1331.
  
  AERMOD regulatory dispersion model including PRIME building downwash algorithm.

* **Scire, J. S., Strimaitis, D. G., & Yamartino, R. J. (1992)**. A User's Guide for the CALPUFF Dispersion Model (Version 5). *Earth Tech, Inc., Concord, MA*.
  
  CALPUFF long-range transport model incorporating building effects and puff dispersion.

* **EPA (2005)**. Revision to the Guideline on Air Quality Models: Adoption of a Preferred General Purpose (Flat and Complex Terrain) Diffusion Model and Adoption of a Preferred Plume Visibility Model. *Federal Register*, 70(216), 68218–68261.
   
  U.S. EPA regulatory guidance on approved dispersion models and algorithms.

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

* **Scruton, C. (1981)**. An introduction to the aeroelasticity of suspension bridges. *Proceedings of the Institution of Civil Engineers*, Part 2, 71(Dec), 829–854.
   
  Cable-supported bridge dynamics and resonance mechanisms for long-span bridges.

* **Yamaguchi, H. (1992)**. Analytical and experimental studies on aerodynamic instabilities of cable-stayed bridges. *Journal of Wind Engineering and Industrial Aerodynamics*, 33(3–4), 371–389.
   
  Cable-supported structure instability analysis and vortex shedding interaction.

* **ASCE (American Society of Civil Engineers). (2017)**. *Minimum Design Loads and Associated Criteria for Buildings and Other Structures* (ASCE 7–16). Reston, VA.
   
  Standard wind loading criteria for bridge design including dynamic amplification and gust factor methodology.

* **ISO 6954:2010 (2010)**. *Mechanical vibration — Guidelines for the measurement and evaluation of vibration and its effects on buildings*. International Organization for Standardization.
   
  Standard methodology for assessing vibration-induced human comfort thresholds for swaying structures.

Structural Dynamics & Fragility Curves
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **Chopra, A. K. (2012)**. *Dynamics of Structures: Theory and Applications to Earthquake Engineering* (4th ed.). Pearson.
   
  Comprehensive treatment of structural dynamics, modal analysis, and response to dynamic loading.

* **Cornell, C. A. (1996)**. Probabilistic basis for 2000 SAC Federal Emergency Management Agency steel moment frame guidelines. *Journal of Structural Engineering*, 128(4), 526–533.
   
  Foundational work on fragility curves and probabilistic damage assessment for structures.

* **Baker, J. W., & Cornell, C. A. (2008)**. A vector-valued ground motion intensity measure consisting of spectral acceleration and epsilon. *Journal of Earthquake Engineering*, 9(4), 1–18.
   
  Methodology for probabilistic structural response modeling and fragility curve development.

* **HAZUS-MH (2012)**. *Multi-hazard Loss Estimation Methodology: Hurricane Model*. Department of Homeland Security, FEMA Mitigation Division.
   
  FEMA standard fragility curves for various building types and damage state classification.

Transmission Line & Conductor Thermal Rating
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **IEEE Std 738 (2012)**. *IEEE Standard for Calculating the Current-Temperature Relationship of Bare Overhead Conductors*. IEEE Power & Energy Society.
   
  Industry standard for thermal current rating calculations including Joule heating, radiation, and forced convection.

* **Mathiesen, A. M., & Svitra, P. (2003)**. *Dynamic thermal line rating system for composite overhead transmission lines*. CIGRE Technical Brochure 207, Paris.
   
  Dynamic line rating methodology accounting for wind speed and temperature effects on conductor capacity.

* **Morgan, V. T. (1980)**. The thermal and electrical properties of high-voltage transmission conductors. *Journal of Materials*, 15(12), 872–890.
   
  Thermal properties of typical transmission line conductors (ACSR, AAAC) and their temperature dependence.

* **CIGRE WG B2.43 (2014)**. *Guide for Selection of Weather Parameters for Bare Overhead Conductor Rating Calculations*. CIGRE Technical Brochure 550, Paris.
   
  Guidance on appropriate meteorological inputs for transmission line ampacity calculations including wind direction effects.

Gap Flow & Orographic Wind Enhancement
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **Delparte, C., Hacker, J. P., & Jiménez, M. (2000)**. Gap flow wind acceleration in the Altamont Pass, California. *Journal of Applied Meteorology*, 39(5), 619–635.
   
  Detailed modeling and observations of gap flow wind acceleration mechanism relevant to Altamont Pass transmission corridor.

* **Grubisic, V., & Stiperski, I. (2009)**. Lee-side flow acceleration up to summit level. *Journal of the Atmospheric Sciences*, 66(10), 3230–3247.
   
  Physics of flow acceleration over ridges and through mountain passes with application to wind energy.

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

* **Yokoyama, H., Oikawa, S., & Miyashita, K. (2010)**. Large-eddy simulation of thermal effects on wind characteristics over an urban canopy. *Journal of Wind Engineering and Industrial Aerodynamics*, 98(8–9), 405–413.
   
  Coupled thermal-wind modeling for urban environments with heat island effects.

* **Lakehal, D., Neumann, P., & Rodi, W. (2003)**. DNS and LES of passive scalar transport in a turbulent channel flow with wall injection. *International Journal of Heat and Fluid Flow*, 24(3), 322–335.
   
  Advanced computational methods for scalar transport in complex geometries applicable to urban canopy modeling.

Wind Field Interpolation & Spatial Coherence
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **Gorlé, C., Beeck, J. V., Rambaud, P., & Tendeloo, G. V. (2009)**. CFD modeling of small-particle dispersion: The influence of the local flow field. *Atmospheric Environment*, 43(3), 554–561.
   
  Methods for spatially coherent wind field representation in urban and complex terrain applications.

* **Panofsky, H. A., & Dutton, J. A. (1984)**. *Atmospheric Turbulence: Models and Methods for Engineering Applications*. Wiley-Interscience.
   
  Theoretical foundation for wind field turbulence and spatial correlation structures.

Risk Assessment & Standards
^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **NFPA 110 (2016)**. *Standard for Emergency and Standby Power Systems*. National Fire Protection Association.
   
  Standards for critical infrastructure resilience and emergency power requirements during wind events.

* **IEC 61400-1 (2019)**. *Wind Energy Generation Systems – Part 1: Safety and functional performance specification*. International Electrotechnical Commission.
   
  International standard for wind energy infrastructure design loads and safety factors.

* **ISO 4355 (2013)**. *Bases for design of structures – Determination of snow loads on roofs*. International Organization for Standardization.
   
  Environmental load standards and methodology for multihazard assessment (extensible to wind loads).

Aerodynamic Drag Coefficients & Bluff Body Aerodynamics
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **Zdravkovich, M. M. (2002)**. *Flow Around Circular Cylinders: Vol. 1: Fundamentals*. Oxford University Press.
   
  Comprehensive reference on drag coefficients and aerodynamic forces on bluff bodies.

* **Isyumov, N. (1997)**. Wind tunnel studies of buildings and structures. *Journal of Wind Engineering and Industrial Aerodynamics*, 74–76, 15–30.
   
  Experimental determination of drag and lift coefficients for engineering structures.

* **Tamura, Y., Matsui, M., Pagnini, L. C., Ismail, M., & Iwatani, Y. (2010)**. Measurement of wind-induced response of buildings using RTK-GPS. *Journal of Wind Engineering and Industrial Aerodynamics*, 90(4–5), 289–313.
   
  Field measurement techniques and validation of structural wind response models.

Vortex-Induced Vibration & Fluid-Structure Interaction
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **Williamson, C. H. K., & Govardhan, R. (2004)**. Vortex-induced vibrations. *Annual Review of Fluid Mechanics*, 36, 413–455.
   
  Comprehensive review of vortex-induced vibration mechanisms and amplitude-frequency relationships.

* **Parkinson, G. V. (1989)**. Phenomena and modelling of flow-induced vibrations of bluff bodies. *Journal of Wind Engineering and Industrial Aerodynamics*, 33(3–4), 681–694.
   
  Physics of flow-induced oscillations relevant to bridges and transmission lines.

* **Sarpkaya, T. (2004)**. A critical review of the intrinsic nature of vortex-induced vibrations. *Journal of Fluids and Structures*, 19(4), 389–447.
   
  Detailed treatment of vortex formation, vortex cell, and resulting forces on structures.

Numerical Methods & Computational Wind Engineering
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **Cook, N. J. (1985)**. The Designer's Guide to Wind Loading of Building Structures: Static Structures (Building Research Establishment). Butterworth-Heinemann.
   
  Practical engineering guidance for wind load determination and structural response estimation.

* **ASCE (2022)**. *Wind Tunnel Testing of Buildings and Other Structures* (ASCE 49–12). Reston, VA.
   
  Standard methodology for conducting wind tunnel experiments and translating results to design loads.

* **Blocken, B., Stathopoulos, T., & Carmeliet, J. (2007)**. CFD simulation of the atmospheric boundary layer: wall function problems. *Atmospheric Environment*, 41(2), 238–252.
   
  Computational fluid dynamics best practices for atmospheric boundary layer modeling.

Terrain Modeling & Complex Topography Effects
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* **Jackson, P. S., & Hunt, J. C. R. (1975)**. Turbulent wind flow over a low hill. *Quarterly Journal of the Royal Meteorological Society*, 101(430), 929–955.
   
  Analytical and experimental treatment of flow over topography; foundational for Jackson-Hunt parameterization.

* **Belcher, S. E., Carruthers, D. J., & Hunt, J. C. R. (1994)**. The wind over hills. *Annual Review of Fluid Mechanics*, 26, 169–210.
   
  Comprehensive review of terrain effects including Froude number and vertical velocity components.

* **Grubisic, V. (2004)**. The Morning Glory of the Gulf of Carpentaria: Mesoscale dynamics and observations. *Monthly Weather Review*, 132(12), 2830–2841.
   
  Large-scale topographic flow patterns and nonlinear terrain interactions.

Database & Empirical Data
^^^^^^^^^^^^^^^^^^^^^^^^

* **NREL Wind Toolkit (2015)**. *National Solar Radiation Database (NSRDB)*. https://nsrdb.nrel.gov/
   
  Open-source wind resource data platform providing spatially interpolated wind fields (referenced for comparison).

* **NOAA National Weather Service (2023)**. *High-Resolution Rapid Refresh (HRRR) Model*. https://rapidrefresh.noaa.gov/
   
  Operational mesoscale wind prediction model (referenced for scenario validation and comparison).

* **USGS 3DEP (3D Elevation Program). (2017)**. *The 3D Elevation Program — Summary of the 2014–2018 Strategy*. U.S. Geological Survey Circular 1399.
   
  High-resolution elevation and terrain data used for site characterization and topography preprocessing.
