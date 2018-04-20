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
    double val;
    complex double valc;

    FILE *outp;
    outp = fopen("Jacobian.dat", "wb");

    //fwrite(&jacob.numDDMbins, 1, sizeof(double), outp);
    //fwrite(&jacob.numSurfacePts, 1, sizeof(double), outp);

    for (int i = 0; i < jacob.numDDMbins * jacob.numSurfacePts; i++) {
        val = jacob.data[i].value;
        fwrite(&val, 1, sizeof(double), outp);
    }
    fclose(outp);
    printf("save Jacobian into file\n");
}