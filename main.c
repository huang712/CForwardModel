#include <time.h>
#include "forwardmodel.h"
#include "cygnss.h"

void Process_DDM(char L1dataFilename[], int sampleIndex, int ddmIndex, int pathType);
//pathType: 0 for default, 1 for folder

int main() {
    char L1dataFilename[1000] = "../../Data/CYGNSSL1/cyg04.ddmi.s20170904-000000-e20170904-235959.l1.power-brcs.a20.d20.nc";
    Process_DDM(L1dataFilename, 80928, 0, 0);

    //char L1dataFilename[1000] = "../../Data/CYGNSSL1/cyg06.ddmi.s20170904-000000-e20170904-235959.l1.power-brcs.a20.d20.nc";
    //Process_DDM(L1dataFilename, 30613, 3); //30613 30619 30700

    //for (int index = 80928; index < 81111; index++){   //80981-81111
    //    Process_DDM(L1dataFilename, index, 0, 1);
    //}
    return 0;
}


void Process_DDM(char L1dataFilename[], int sampleIndex, int ddmIndex, int pathType){
    struct CYGNSSL1 l1data;
    readL1data(L1dataFilename, sampleIndex, ddmIndex, &l1data);
    if(l1data.quality_flags != 0) return; //skip data of quality issue
    //l1data.ddm_sp_delay_row = l1data.ddm_sp_delay_row-0.5;
    printf("sampleIndex = %d, quality_flags = %d\n", sampleIndex, l1data.quality_flags);
    printf("GPS PRN = %d\n", l1data.prn_code);
    printf("sp delay row = %f\n", l1data.ddm_sp_delay_row);
    printf("sp doppler col = %f\n", l1data.ddm_sp_dopp_col);
    //printf("peak delay row = %d\n", l1data.ddm_peak_delay_row);
    //printf("peak doppler col = %d\n", l1data.ddm_peak_dopp_col);
    struct metadata meta;
    struct powerParm pp;
    struct inputWindField iwf;
    struct Geometry geom;
    struct DDMfm ddm_fm;
    struct Jacobian jacob;

    printf("\n");
    printf("Initialize input/output structure...\n");
    char windFileName[1000] = "../../Data/HWRF/irma11l.2017090418.hwrfprs.synoptic.0p125.f005.nc";
    init_metadata(l1data, &meta);
    init_powerParm(l1data, &pp);
    init_inputWindField_synoptic(windFileName, &iwf);
    init_Geometry(l1data, &geom);
    init_DDM(l1data, &ddm_fm);
    init_Jacobian(&jacob);

    //int index = 476 * 721 +410;
    //printf("lat = %f, lon = %f, wind = \n",iwf.data[index].lat_deg,iwf.data[index].lon_deg,iwf.data[index].windSpeed_U10_ms);
    //printf("\n");
    double start, end;
    start = clock();
    forwardModel(meta, pp, iwf, geom, &ddm_fm, &jacob);
    end =clock();
    printf("Forward model running time: %f seconds\n", (end-start)/CLOCKS_PER_SEC);

    //int pathType = 1; //0 to save in defaut path; 1 to save in folder
    DDMobs_saveToFile(l1data, sampleIndex,pathType);
    DDMfm_saveToFile(ddm_fm, sampleIndex,pathType);
    //Jacobian_saveToFile(jacob);

    printf("\n");
    printf("ddm = %e\n",ddm_fm.data[0].power);
    //printf("delay = %e\n",ddm_fm.data[0].delay);
    //printf("Doppler = %e\n",ddm_fm.data[0].Doppler);

    free(pp.data);
    free(iwf.data);
    free(ddm_fm.data);
    free(jacob.data);

    printf("End");
}