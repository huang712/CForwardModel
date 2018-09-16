//
// Created by Feixiong Huang on 1/21/18.
//

#include "forwardmodel.h"
#include "complex.h"

void DDMfm_saveToFile(struct DDMfm ddm_fm, int index, int pathType) {

    double val;
    char filename[50];

    FILE *outp;
    switch(pathType){
        case 0:
            outp = fopen("DDMfm.dat","wb");
            break;
        case 1:
            sprintf(filename, "DDMfm/DDMfm%d.dat", index);
            outp = fopen(filename, "wb");
            break;
    }

    for (int i = 0; i < ddm_fm.numDelaybins * ddm_fm.numDopplerbins; i++) {
        val = ddm_fm.data[i].power;
        fwrite(&val, 1, sizeof(double), outp);
    }
    fclose(outp);
    printf("save FM DDM into file\n");
}

void Jacobian_saveToFile(struct Jacobian jacob){
    double val,lat,lon;
    //complex double valc;

    FILE *outp,*outp1,*outp2;
    outp = fopen("Jacobian.dat", "wb");
    outp1 = fopen("Jacobian_lat.dat", "wb");
    outp2 = fopen("Jacobian_lon.dat", "wb");

    double numDDMbins,numSurfacePts;
    numDDMbins = (double)jacob.numDDMbins;
    numSurfacePts = (double)jacob.numSurfacePts;

    fwrite(&numDDMbins, 1, sizeof(double), outp);
    fwrite(&numDDMbins, 1, sizeof(double), outp1);
    fwrite(&numDDMbins, 1, sizeof(double), outp2);
    fwrite(&numSurfacePts, 1, sizeof(double), outp);
    fwrite(&numSurfacePts, 1, sizeof(double), outp1);
    fwrite(&numSurfacePts, 1, sizeof(double), outp2);

    for (int i = 0; i < jacob.numDDMbins * jacob.numSurfacePts; i++) {
        val = jacob.data[i].value;
        //if(val >0 || abs(val)>1){
        if(abs(val)>1){
            printf("something wrong!! i= %d, H=%e\n",i,val);
            //val=-1e-30;
        }

        lat = jacob.data[i].lat_deg;
        lon = jacob.data[i].lon_deg;
        fwrite(&val, 1, sizeof(double), outp);
        fwrite(&lat, 1, sizeof(double), outp1);
        fwrite(&lon, 1, sizeof(double), outp2);
    }
    fclose(outp);
    fclose(outp1);fclose(outp2);
    printf("save Jacobian into file\n");
}