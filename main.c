#include <time.h>
#include "forwardmodel.h"
#include "cygnss.h"
// I add it 
// I add second
int main() {
    char L1dataFilename[1000] = "../../Data/CYGNSSL1/cyg06.ddmi.s20170904-000000-e20170904-235959.l1.power-brcs.a20.d20.nc";
    int sampleIndex = 30613;  //30613   30619
    int ddm_index = 3;
    struct CYGNSSL1 l1data;
    readL1data(L1dataFilename, sampleIndex, ddm_index, &l1data);

    struct metadata meta;
    struct powerParm pp;
    struct inputWindField iwf;
    struct Geometry geom;
    struct DDMfm ddm_fm;
    struct Jacobian jacob;

    printf("\n");
    printf("Initialize input/output structure...\n");
    char windFileName[1000] = "../../Data/HWRF/irma11l.2017090406.hwrfprs.core.0p02.f003.nc";
    init_metadata(l1data, &meta);
    init_powerParm(l1data, &pp);
    init_inputWindField(windFileName, &iwf);
    init_Geometry(l1data, &geom);
    init_DDM(l1data, &ddm_fm);
    init_Jacobian(&jacob);

    printf("\n");
    double start, end;
    start = clock();
    forwardModel(meta, pp, iwf, geom, &ddm_fm, &jacob);
    end =clock();
    printf("Forward model running time: %f seconds\n", (end-start)/CLOCKS_PER_SEC);

    DDMobs_saveToFile(l1data);
    DDMfm_saveToFile(ddm_fm);
    //Jacobian_saveToFile(jacob);

    printf("\n");
    printf("ddm = %e\n",ddm_fm.data[0].power);
    printf("ddm = %e\n",ddm_fm.data[0].delay);
    printf("ddm = %e\n",ddm_fm.data[0].Doppler);

    //printf("H = %e\n",jacob.data[100].value);

    free(pp.data);
    free(iwf.data);
    free(ddm_fm.data);
    free(jacob.data);
    
    printf("End");

    return 0;
}


