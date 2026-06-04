#include "wind_solver_app.H"
#include <AMReX.H>

int main(int argc, char* argv[])
{
    amrex::Initialize(argc, argv);
    {
        WindSolverApp app;
        app.initialize();
        app.execute();
    }
    amrex::Finalize();
    return 0;
}
