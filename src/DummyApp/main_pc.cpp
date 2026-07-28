#include "DummyApp.h"

#include <Eng/renderer/BlueNoise_Wolfe.h>
#include <Eng/renderer/BlueNoise_Ours.h>

int main(int argc, char *argv[]) {

    //Eng::Generate1D_STBN<4>(42, false);
    //Eng::Generate1D_TCBN_VC<4>(42, true);

    //Eng::Generate2D_TCBN<6, Eng::eSpatialFilter::Gauss, Eng::eTemporalFilter::TruncatedLinear>(42);
    //Eng::Generate2D_TCBN<5>(42);
    //Eng::Generate2D_TCBN<4, Eng::eSpatialFilter::Gauss, Eng::eTemporalFilter::TruncatedLinear>(42);

    //return 0;

    return DummyApp().Run(argc, argv);
}
