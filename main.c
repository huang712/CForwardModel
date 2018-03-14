#include <time.h>
#include "forwardmodel.h"
#include "cygnss.h"

void Process_DDM(char L1dataFilename[], int sampleIndex, int ddmIndex);

int main() {
    char L1dataFilename[1000] = "../../Data/CYGNSSL1/cyg06.ddmi.s20170904-000000-e20170904-235959.l1.power-brcs.a20.d20.nc";
    Process_DDM(L1dataFilename, 30613, 3);

    return 0;
}


void Process_DDM(char L1dataFilename[], int sampleIndex, int ddmIndex){
    struct CYGNSSL1 l1data;
    readL1data(L1dataFilename, sampleIndex, ddmIndex, &l1data);

    struct metadata meta;
    struct powerParm pp;
    struct inputWindField iwf;
    struct Geometry geom;
    struct DDMfm ddm_fm;
    struct Jacobian jacob;

    printf("\n");
    printf("Initialize input/output structure...\n");
    char windFileName[1000] = "../../Data/HWRF/irma11l.2017090406.hwrfprs.synoptic.0p125.f003.nc";
    init_metadata(l1data, &meta);
    init_powerParm(l1data, &pp);
    init_inputWindField_synoptic(windFileName, &iwf);
    init_Geometry(l1data, &geom);
    init_DDM(l1data, &ddm_fm);
    init_Jacobian(&jacob);

    printf("\n");
    double start, end;
    start = clock();
    forwardModel(meta, pp, iwf, geom, &ddm_fm, &jacob);
    end =clock();
    printf("Forward model running time: %f seconds\n", (end-start)/CLOCKS_PER_SEC);

    //DDMobs_saveToFile(l1data);
    //DDMfm_saveToFile(ddm_fm);
    //Jacobian_saveToFile(jacob);

    printf("\n");
    printf("ddm = %e\n",ddm_fm.data[0].power);
    printf("delay = %e\n",ddm_fm.data[0].delay);
    printf("Doppler = %e\n",ddm_fm.data[0].Doppler);

    free(pp.data);
    free(iwf.data);
    free(ddm_fm.data);
    free(jacob.data);

    printf("End");
}