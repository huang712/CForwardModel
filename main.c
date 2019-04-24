#include <math.h>
#include <time.h>
#include <string.h>
#include "forwardmodel.h"
#include "cygnss.h"

void Process_DDM(char windFilename[], char HWRFtype[], char L1dataFilename[], int sampleIndex, int ddmIndex, int pathType); //pathType: 0 for default, 1 for folder
double DDM_binshift_corr(char L1dataFilename[], int sampleIndex, int ddmIndex, double shift);
double corr2(double *A, double *B, int n);
double find_opt_delayshift(char L1dataFilename[], int sampleIndex, int ddmIndex);

void FiniteDiff(char windFilename[], char HWRFtype[], char L1dataFilename[], int sampleIndex, int ddmIndex, int pathType);

int main() {
    ///// Maria 20170923 1800
    /*
    char windFilename[1000] = "../../Data/Maria2017/maria15l.2017092318.hwrfprs.synoptic.0p125.f000.nc";
    char L1dataFilename[1000] = "../../Data/Maria2017/cyg05.ddmi.s20170923-000000-e20170923-235959.l1.power-brcs.a21.d21.nc";
    int pathType = 1; //0 for current; 1 for folder
    int ddmIndex = 0;
    for (int index = 65059; index < 65060; index++){   //80928-81111
        Process_DDM(windFilename,"synoptic", L1dataFilename, index, ddmIndex, pathType);
    }
    */

    ///// Irma 20170904 2300

    //Irma ddmIndex=0, 80928-81111, eye = 81095
    //char windFilename[1000] = "../../Data/Irma2017/irma201709042240shift.nc";  //check NLAT NLON in initilization.c
    char windFilename[1000] = "../../Data/Irma2017/irma11l.2017090418.hwrfprs.synoptic.0p125.f005.uv.nc";
    char L1dataFilename[1000] = "../../Data/Irma2017/cyg04.ddmi.s20170904-000000-e20170904-235959.l1.power-brcs.a21.d21.nc";
    //char L1dataFilename[1000] = "../../Data/Irma2017/cyg04.ddmi.s20170904-000000-e20170904-235959.l1.power-brcs.sand031.nc";

    //Process_DDM(windFilename, "synoptic", L1dataFilename, 81096, 0, 0);
    //FiniteDiff(windFilename, "synoptic", L1dataFilename, 81096, 0, 0);

    int pathType = 1; //0 for current; 1 for folder
    int ddmIndex = 0;
    for (int index = 81100; index < 81101; index++){   //80928-81111  81095
       Process_DDM(windFilename,"synoptic", L1dataFilename, index, ddmIndex, pathType);
    }


    ///// Gita 20180212 1400
    //Gita ddmIndex=2 50571-50730, eye = 50640
    //char windFilename[1000] = "../../Data/Gita2018/gita09p.2018021212.hwrfprs.synoptic.0p125.f002.uv.nc";
    //char windFilename[1000] = "../../Data/Gita2018/gita09p.2018021212.hwrfprs.core.0p02.f002.uv.nc";
    //char L1dataFilename[1000] = "../../Data/Gita2018/cyg01.ddmi.s20180212-000000-e20180212-235959.l1.power-brcs.a20.d20.nc";
    //char L1dataFilename[1000] = "../../Data/Gita2018/cyg01.ddmi.s20180212-000000-e20180212-235959.l1.power-brcs.sand031.nc";

    //Process_DDM(windFilename, "synoptic", L1dataFilename, 50640, 2, 0);

    //for (int index = 50571; index < 50730; index++){   //80981-81111
    //    Process_DDM(windFilename,"synoptic", L1dataFilename, index, 2, 1);
    //}
    ////////////////////////

    /*
    double shift, f_index;  // calculate optimal delay bin shift
    int k = 0;
    FILE *outp = fopen("Delayshift.dat","ab+");;
    for (int index = 50720; index < 50730; index++){  //80981-81111    50571-50730
        printf("index = %d\n",index);
        shift=find_opt_delayshift(L1dataFilename, index, 2);
        f_index = (double)index;
        fwrite(&f_index, 1, sizeof(double), outp);
        fwrite(&shift, 1, sizeof(double), outp);
        k++;
    }
    fclose(outp);
    */
    return 0;
}

void Process_DDM(char windFilename[], char HWRFtype[], char L1dataFilename[], int sampleIndex, int ddmIndex, int pathType){
    struct CYGNSSL1 l1data;
    readL1data(L1dataFilename, sampleIndex, ddmIndex, &l1data);
    if(l1data.quality_flags != 0) {
        printf("quality flags not 0\n");
        return; //skip data of quality issue
    }

    printf("sampleIndex = %d, quality_flags = %d\n", sampleIndex, l1data.quality_flags);
    printf("GPS PRN = %d\n", l1data.prn_code);
    //l1data.ddm_sp_delay_row = l1data.ddm_sp_delay_row + 0.5;
    printf("sp delay row = %f, sp doppler col = %f\n", l1data.ddm_sp_delay_row,l1data.ddm_sp_dopp_col);
    printf("sp lat = %f, lon = %f\n",l1data.sp_lat,l1data.sp_lon);
    printf("ant = %d\n",l1data.ddm_ant);
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

    init_metadata(l1data, &meta);
    init_powerParm(l1data, &pp);
    if(strcmp(HWRFtype,"core") == 0) init_inputWindField_core(windFilename, &iwf);
    else if(strcmp(HWRFtype,"synoptic") == 0) init_inputWindField_synoptic(windFilename, &iwf);
    init_Geometry(l1data, &geom);
    init_DDM(l1data, &ddm_fm);
    init_Jacobian(&jacob);

    double start, end;
    start = clock();
    forwardModel(meta, pp, iwf, geom, &ddm_fm, &jacob,0);
    end =clock();
    printf("Forward model running time: %f seconds\n", (end-start)/CLOCKS_PER_SEC);

    printf("ddm 50= %e\n",ddm_fm.data[50].power);
    printf("H = %e\n",jacob.data[4912].value);

    DDMobs_saveToFile(l1data, sampleIndex,pathType);
    DDMfm_saveToFile(ddm_fm, sampleIndex,pathType);
    //Jacobian_saveToFile(jacob);
    //PtsVec_saveToFile(jacob);

    free(pp.data);
    free(iwf.data);
    free(ddm_fm.data);
    free(jacob.data);

    printf("END\n");
    printf("\n");
} // end of process_DDM

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

    //char windFileName[1000] = "../../Data/HWRF/irma11l.2017090418.hwrfprs.synoptic.0p125.f005.nc";
    char windFileName[1000] = "../../Data/Gita2018/gita09p.2018021212.hwrfprs.core.0p02.f002.uv.nc";
    init_metadata(l1data, &meta);
    init_powerParm(l1data, &pp);
    init_inputWindField_synoptic(windFileName, &iwf);
    init_Geometry(l1data, &geom);
    init_DDM(l1data, &ddm_fm);
    init_Jacobian(&jacob);

    forwardModel(meta, pp, iwf, geom, &ddm_fm, &jacob,0);

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

void FiniteDiff(char windFilename[], char HWRFtype[], char L1dataFilename[], int sampleIndex, int ddmIndex, int pathType){
    struct CYGNSSL1 l1data;
    readL1data(L1dataFilename, sampleIndex, ddmIndex, &l1data);
    if(l1data.quality_flags != 0) return; //skip data of quality issue

    struct metadata meta;
    struct powerParm pp;
    struct inputWindField iwf,iwf1;
    struct Geometry geom;
    struct DDMfm ddm_fm0,ddm_fm1;
    struct Jacobian jacob;

    printf("\n");
    printf("Initialize input/output structure...\n");

    init_metadata(l1data, &meta);
    init_powerParm(l1data, &pp);
    init_inputWindField_synoptic(windFilename, &iwf);
    init_Geometry(l1data, &geom);
    init_DDM(l1data, &ddm_fm0);
    init_DDM(l1data, &ddm_fm1);
    init_Jacobian(&jacob);

    int i,j;
    double start, end;
    start = clock();

    forwardModel(meta, pp, iwf, geom, &ddm_fm0, &jacob,1);
    Jacobian_saveToFile(jacob);
    int numPts_LL = jacob.numPts_LL;

    double **H_FD; //H matrix by finite difference H[187][numPts_LL]
    H_FD = (double**)calloc(187,sizeof(double*));
    for (i = 0; i < 187; i++){
        H_FD[i] = (double*)calloc(numPts_LL,sizeof(double));
    }

    int Pts_index;
    double u,v;
    for (i=0;i<numPts_LL;i++){
        init_inputWindField_synoptic(windFilename, &iwf);
        Pts_index = jacob.Pts_ind_vec[i];
        iwf.data[Pts_index].windSpeed_ms +=0.00001;
        forwardModel(meta, pp, iwf, geom, &ddm_fm1, &jacob,0);
        for (j = 0;j < 187; j++){
            H_FD[j][i]=(ddm_fm1.data[j].power-ddm_fm0.data[j].power)/0.00001;
        }
    }

    end =clock();
    printf("Finite Diffence running time: %f seconds\n", (end-start)/CLOCKS_PER_SEC);

    //DDMfm_saveToFile(ddm_fm, sampleIndex,pathType);
    //Jacobian_saveToFile(jacob);
    //PtsVec_saveToFile(jacob);

    printf("\n");
    printf("H = %e\n",H_FD[51][27]);
    printf("num point in LL = %d\n",jacob.numPts_LL);

    FILE *outp = fopen("H_FD.dat", "wb");
    for (j = 0;j< numPts_LL;j++) {
        for (i = 0; i < 187; i++){
            fwrite(&H_FD[i][j], sizeof(double), 1, outp);
        }
    }
    fclose(outp);

    for (i = 0; i < 187; i++){
        free(H_FD[i]);
    }
    free(H_FD);

    free(pp.data);
    free(iwf.data);
    free(ddm_fm0.data);
    free(jacob.data);

    printf("END\n");
    printf("\n");
} // end of FiniteDiff