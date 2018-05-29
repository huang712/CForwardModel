#include <time.h>
#include "forwardmodel.h"
#include "cygnss.h"
#include "math.h"

void Process_DDM(char L1dataFilename[], int sampleIndex, int ddmIndex, int pathType); //pathType: 0 for default, 1 for folder
double DDM_binshift_corr(char L1dataFilename[], int sampleIndex, int ddmIndex, double shift);
double corr2(double *A, double *B, int n);
double find_opt_delayshift(char L1dataFilename[], int sampleIndex, int ddmIndex);

int main() {
    char L1dataFilename[1000] = "../../Data/CYGNSSL1/cyg04.ddmi.s20170904-000000-e20170904-235959.l1.power-brcs.a20.d20.nc";

    /*
    double shift, f_index;  // calculate optimal delay bin shift
    int k = 0;
    FILE *outp = fopen("Delayshift.dat","ab+");;
    for (int index = 81090; index < 81111; index++){  //80981-81111
        printf("index = %d\n",index);
        shift=find_opt_delayshift(L1dataFilename, index, 0);
        f_index = (double)index;
        fwrite(&f_index, 1, sizeof(double), outp);
        fwrite(&shift, 1, sizeof(double), outp);
        k++;
    }
    fclose(outp);
    */

    Process_DDM(L1dataFilename, 81096, 0, 0);

    //for (int index = 80928; index < 81111; index++){   //80981-81111
    //    Process_DDM(L1dataFilename, index, 0, 1);
    //}
    return 0;
}

void Process_DDM(char L1dataFilename[], int sampleIndex, int ddmIndex, int pathType){
    struct CYGNSSL1 l1data;
    readL1data(L1dataFilename, sampleIndex, ddmIndex, &l1data);
    if(l1data.quality_flags != 0) return; //skip data of quality issue

    printf("sampleIndex = %d, quality_flags = %d\n", sampleIndex, l1data.quality_flags);
    printf("GPS PRN = %d\n", l1data.prn_code);
    printf("sp delay row = %f\n", l1data.ddm_sp_delay_row);
    printf("sp doppler col = %f\n", l1data.ddm_sp_dopp_col);
    //printf("ant = %d\n",l1data.ddm_ant);
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

    printf("END\n");
    printf("\n");
}

double find_opt_delayshift(char L1dataFilename[], int sampleIndex, int ddmIndex){
    // return the optimal shift in DDM model
    double correlation, temp;
    int shift_index;
    double shift[11]={-0.5,-0.4,-0.3,-0.2,-0.1, 0 ,0.1,0.2,0.3,0.4,0.5};
    struct CYGNSSL1 l1data;
    readL1data(L1dataFilename, sampleIndex, 0, &l1data);
    if(l1data.quality_flags != 0){
        printf("skip by quality control\n");
        return NAN; //skip data of quality issue
    }
    shift_index = 0;//optimal shift
    correlation = 0;
    for (int i =0; i<11;i++){
        temp = DDM_binshift_corr(L1dataFilename, sampleIndex, 0, shift[i]);
        if (temp>correlation){
            correlation = temp;
            shift_index = i;
        }
    }
    printf("shift = %f, correlation = %f\n",shift[shift_index], correlation);
    printf("\n");
    return shift[shift_index];
}

double DDM_binshift_corr(char L1dataFilename[], int sampleIndex, int ddmIndex, double shift){
    //return the correlation between shifted DDM and observed DDM
    struct CYGNSSL1 l1data;
    readL1data(L1dataFilename, sampleIndex, ddmIndex, &l1data);
    if(l1data.quality_flags != 0) return NAN; //skip data of quality issue

    l1data.ddm_sp_delay_row = l1data.ddm_sp_delay_row+shift;
    struct metadata meta;
    struct powerParm pp;
    struct inputWindField iwf;
    struct Geometry geom;
    struct DDMfm ddm_fm;
    struct Jacobian jacob;

    char windFileName[1000] = "../../Data/HWRF/irma11l.2017090418.hwrfprs.synoptic.0p125.f005.nc";
    init_metadata(l1data, &meta);
    init_powerParm(l1data, &pp);
    init_inputWindField_synoptic(windFileName, &iwf);
    init_Geometry(l1data, &geom);
    init_DDM(l1data, &ddm_fm);
    init_Jacobian(&jacob);

    forwardModel(meta, pp, iwf, geom, &ddm_fm, &jacob);

    free(pp.data);
    free(iwf.data);
    free(ddm_fm.data);
    free(jacob.data);

    double ddm_obs[187], ddm_fm0[187];
    for (int i=0;i<17;i++){
        for (int j=0;j<11;j++){
            ddm_obs[17*j+i]=l1data.DDM_power[i][j];
        }
    }

    for (int i=0;i<187;i++){
        ddm_fm0[i]=ddm_fm.data[i].power;
    }

    int num_effbin = 0;
    int effbin_index[187];
    for (int i=0;i<187;i++){
        if(ddm_obs[i]>1e-18){
            effbin_index[num_effbin] = i;
            num_effbin++;
        }
    }

    double *ddm_obs_eff = (double *)calloc(num_effbin, sizeof(double));
    double *ddm_fm0_eff = (double *)calloc(num_effbin, sizeof(double));
    for (int i=0;i<num_effbin;i++){
        ddm_obs_eff[i]=ddm_obs[effbin_index[i]];
        ddm_fm0_eff[i]=ddm_fm0[effbin_index[i]];
    }

    double correlation;
    correlation = corr2(ddm_obs_eff,ddm_fm0_eff,num_effbin);

    free(ddm_obs_eff);
    free(ddm_fm0_eff);
    return correlation;
}

double corr2(double *A, double *B, int n){
    //correlation coefficient of array A and B with length n; same as MATLAB corr2()
    double A_ave, B_ave;
    A_ave = 0;
    B_ave = 0;
    for (int i=0;i<n;i++){
         A_ave = A_ave + A[i];
         B_ave = B_ave + B[i];
    }
    A_ave = A_ave/n;
    B_ave = B_ave/n;

    double nom, den, A1, B1;
    nom = 0;
    A1 = 0;
    B1 = 0;
    for (int i=0;i<n;i++){
        nom = nom + (A[i]-A_ave)*(B[i]-B_ave);
        A1= A1 + pow(A[i]-A_ave,2);
        B1= B1 + pow(B[i]-B_ave,2);
    }
    den = sqrt (A1*B1);

    return nom/den;
}