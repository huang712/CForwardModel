//
// Created by Feixiong Huang on 10/22/17.
//


//***************************************************************************
// This file is part of the CYGNSS E2ES.
//
// CYGNSS E2ES is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// CYGNSS E2ES is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with CYGNSS E2ES.  If not, see <http://www.gnu.org/licenses/>.
//
//---------------------------------------------------------------------------
//
// The DDM parameters are stored in a global structure "ddm", while buffers for
// the actual DDM, the ambiguity function, etc are in separate globals, "DDM",
// "DDM_avg", "DDM_amb", etc ... The parameters are read in using the
// "ddm_initializeFromConfigFile" function, which also allocates memory for the
// DDM and precalculates the ambiguity function.  Then one calls various functions
// to assemble the DDM.  See ddm_* calls from main.c
//
//****************************************************************************/

#include "gnssr.h"
#include "forwardmodel.h"

void ddm_initialize(struct metadata meta) {

    //  Initialization of DDM:  Read in DDM params from config file and alloc
    //  and initialize DDM buffers

    // reads DDM parameters from the configuration file into global struct
    ddm.numDelayBins         = meta.numDelaybins;
    ddm.numDoppBins          = meta.numDopplerbins;
    ddm.delayOffset_bins     = meta.specular_delayBinIdx; //
    ddm.dopplerOffset_bins   = meta.specular_dopplerBinIdx;
    ddm.dopplerRes_Hz        = meta.dopplerRes_Hz;
    ddm.delayRes_chips       = meta.delayRez_chips;
    ddm.cohIntegrationTime_s = 0.001;

    ddm.numBins              = ddm.numDelayBins * ddm.numDoppBins;//160000
    ddm.chipsPerSec          = 1.023e6;
    ddm.refPower_dB          = -200; // when plotting log, this is what is used for log10(0)

    // allocate & init DDM buffers
    _ddm_alloc(&DDM,      ddm.numBins);
    _ddm_alloc(&H,        ddm.numBins);
    _ddm_alloc(&DDM_temp, ddm.numBins);
    _ddm_alloc(&DDM_avg,  ddm.numBins);
    _ddm_alloc(&DDM_amb,  ddm.numBins);
    _ddm_alloc(&DDM_amb1, ddm.numBins);
    _ddm_alloc(&DDM_amb2, ddm.numBins);
    _ddm_alloc(&DDM_store,ddm.numBins);

    _ddm_zero( DDM,      ddm.numBins );
    _ddm_zero( H,        ddm.numBins );
    _ddm_zero( DDM_temp, ddm.numBins );
    _ddm_zero( DDM_avg,  ddm.numBins );
    _ddm_zero( DDM_amb,  ddm.numBins );
    _ddm_zero( DDM_amb1, ddm.numBins );
    _ddm_zero( DDM_amb2, ddm.numBins );
    _ddm_zero( DDM_store,ddm.numBins );

    // optimize FFT based on requested DDM size
    ddm.FFTWPLAN  = fftw_plan_dft_2d(ddm.numDoppBins, ddm.numDelayBins, (fftw_complex*) DDM, (fftw_complex*) DDM, FFTW_FORWARD, FFTW_MEASURE);
    ddm.IFFTWPLAN = fftw_plan_dft_2d(ddm.numDoppBins, ddm.numDelayBins, (fftw_complex*) DDM, (fftw_complex*) DDM, FFTW_BACKWARD, FFTW_MEASURE);
    h.FFTWPLAN  = fftw_plan_dft_2d(ddm.numDoppBins, ddm.numDelayBins, (fftw_complex*) H, (fftw_complex*) H, FFTW_FORWARD, FFTW_MEASURE);
    h.IFFTWPLAN = fftw_plan_dft_2d(ddm.numDoppBins, ddm.numDelayBins, (fftw_complex*) H, (fftw_complex*) H, FFTW_BACKWARD, FFTW_MEASURE);


    // precalculate and store the ambiguity function
    ddm_initAmbFuncBuffers();

    // calculate the thermal noise power
    ddm_initThermalNoise(meta);

}

void ddm_cleanup(void){

    // free DDM buffers
    _ddm_free(DDM);
    _ddm_free(DDM_temp);
    _ddm_free(DDM_avg);
    _ddm_free(DDM_amb);
    _ddm_free(DDM_amb1);
    _ddm_free(DDM_amb2);
    _ddm_free(DDM_store);

    fftw_destroy_plan(ddm.FFTWPLAN);
    fftw_destroy_plan(ddm.IFFTWPLAN);
}

/****************************************************************************/
//  Surface Binning and Mapping to DDM
/****************************************************************************/

void ddm_binSurface(void) {
    // loop over surface and use the delay and Doppler value to determine what
    // DDM bin that surface patch maps to
    int delayBin,dopplerBin;
    for(int i=0; i<surface.numGridPts; i++) {

        delayBin   = (int)floor(surface.data[i].delay_s * ddm.chipsPerSec / ddm.delayRes_chips) + ddm.delayOffset_bins;
        dopplerBin = (int)floor(surface.data[i].doppler_Hz/ddm.dopplerRes_Hz) + ddm.dopplerOffset_bins;

        if((dopplerBin < ddm.numDoppBins) && (dopplerBin >= 0) &&
           (delayBin < ddm.numDelayBins) && (delayBin >= 0 )) {
            surface.data[i].bin_index = DDMINDEX(dopplerBin, delayBin);
        }
        else
            surface.data[i].bin_index = -1;
    }
}


void ddm_Hmatrix(struct metadata meta, struct Jacobian *jacob){

    unsigned height = (unsigned) surface.numGridPts;  //8100
    unsigned width  = (unsigned) ddm.numBins;	//160000
    int startDelay_bin    = meta.resample_startBin[0];
    int startDoppelr_bin  = meta.resample_startBin[1];
    int resDelay_bins     = meta.resample_resolution_bins[0];
    int resDoppelr_bins   = meta.resample_resolution_bins[1];
    int numDelayBins      = meta.resample_numBins[0];
    int numDoppelrBins    = meta.resample_numBins[1];


    double temp = 1.0 * numDelayBins * numDoppelrBins;

    _ddm_zero( H, width );

    int surface_index;
    int i=0;
    for(int m = 0 ; m < surface.numGridPtsX/10 ; m = m + 1)
    {
        for(int n = 0 ; n < surface.numGridPtsY/10 ; n = n + 1)
        {
            surface_index = (m * 10 + 4) * surface.numGridPtsY + n * 10 + 4; //resolution from 1km to 10km
            if (surface.data[surface_index].bin_index >= 0){
                H[surface.data[surface_index].bin_index] = surface.data[surface_index].total_dP;
                ddm_convolveH_FFT(2);

                for (int k=startDoppelr_bin; k < (numDoppelrBins*resDoppelr_bins + startDoppelr_bin); k+=resDoppelr_bins) {
                    for (int l=startDelay_bin; l < (numDelayBins*resDelay_bins + startDelay_bin); l+=resDelay_bins) {
                        jacob->data[i].value = creal(H[DDMINDEX(k,l)]) * 100;//H is derivative respect to pixel in 10km resolution
                        i=i+1;
                    }
                }
                _ddm_zero( H, width );
            }
        }
    }

    /* H in 1km resolution
    for(int m = 0 ; m < surface.numGridPtsX ; m = m + 1)
    {
        for(int n = 0 ; n < surface.numGridPtsY ; n = n + 1)
        {
            surface_index = m * surface.numGridPtsY + n; //1km reoslution
            if (surface.data[surface_index].bin_index >= 0){
                H[surface.data[surface_index].bin_index] = surface.data[surface_index].total_dP;
                ddm_convolveH_FFT(2);

                for (int k=startDoppelr_bin; k < (numDoppelrBins*resDoppelr_bins + startDoppelr_bin); k+=resDoppelr_bins) {
                    for (int l=startDelay_bin; l < (numDelayBins*resDelay_bins + startDelay_bin); l+=resDelay_bins) {
                        jacob->data[i].value = creal(H[DDMINDEX(k,l)]) ;
                        i=i+1;
                    }
                }
                _ddm_zero( H, width );
            }
        }
    }
     */

}


void ddm_mapSurfaceToDDM(void) {
    // creates a DDM by looping over the surface and adding the scattered
    // power/complex value to a particular DDM bin
    _ddm_zero( DDM, ddm.numBins );
    for(int i=0; i<surface.numGridPts; i++){
        if (surface.data[i].bin_index >= 0){
            DDM[surface.data[i].bin_index] += surface.data[i].total;
        }
    }
}

void ddm_mapRegionToDDM(void) {
    // Averages markers to produce DDM
    _ddm_zero( DDM,      ddm.numBins );
    _ddm_zero( DDM_temp, ddm.numBins );
    for(int i=0; i<surface.numGridPts; i++){
        if (surface.data[i].bin_index >= 0){
            //DDM_temp[surface.data[i].bin_index] += 1;
            DDM[surface.data[i].bin_index]  = surface.data[i].regionMarker;
        }
    }
    //for (int i = 0; i < ddm.numBins; i++)
    //    if( cabs(DDM_temp[surface.data[i].bin_index]) > 0 )
    //        DDM[surface.data[i].bin_index] /= DDM_temp[surface.data[i].bin_index];
}

void ddm_setSingleBin(int dopplerBin, int delayBin) {
    // test case for debugging.  Just sets single bin of DDM to one
    // and all the others zero  (good way to plot the amb func)

    _ddm_zero( DDM, ddm.numBins );
    if((dopplerBin < ddm.numDoppBins) && (dopplerBin >= 0) &&
       (delayBin < ddm.numDelayBins) && (delayBin >= 0 ))
        DDM[DDMINDEX(dopplerBin, delayBin)] = 1;
    else{
        fprintf(errPtr,"Error: requested bin out of range in ddm_setSingleBin\n");
        exit(1);
    }
}

void ddm_setBox(int centerDopplerBin, int centerDelayBin, int dopplerHalfWidth, int delayHalfWidth) {
    // test case for debugging.  Just sets a rect region of DDM to one
    // and all the others zero  (for use with ddm_mapDDMToSurface)

    for (int l=0; l < ddm.numDelayBins; l++) {
        for (int k=0; k < ddm.numDoppBins; k++) {
            if( ( abs(k - centerDopplerBin) < dopplerHalfWidth ) && ( abs(l - centerDelayBin) < delayHalfWidth )  )
                DDM[DDMINDEX(k, l)] = 1;
        }
    }
}

void ddm_mapDDMToSurface(void) {
    // performs the "inverse" of ddm_mapSurfaceToDDM, for testing the spatial
    // extent of certain DDM bins.  Use ddm_setSingleBin or ddm_setBox
    // before calling this.

    for(int i=0; i<surface.numGridPts; i++){
        if (surface.data[i].bin_index >= 0){
            surface.data[i].total = DDM[surface.data[i].bin_index];
        }else
            surface.data[i].total = 0;
    }

}


/****************************************************************************/
//  FFTs for convolution (uses FFTW)
/****************************************************************************/

void ddm_convolveFFT(int ambFuncType){   //type=2
    // convolves the working DDM with the ambiguity function.  Different
    // versions of the amb func are used for different purposes
    ddm_fft();
    switch (ambFuncType) {
        case 0:  for(int i = 0; i<ddm.numBins; i++) { DDM[i] = DDM_amb[i]  * DDM[i]; } break;
        case 1:  for(int i = 0; i<ddm.numBins; i++) { DDM[i] = DDM_amb1[i] * DDM[i]; } break;
        case 2:  for(int i = 0; i<ddm.numBins; i++) { DDM[i] = DDM_amb2[i] * DDM[i]; } break;
        default: fprintf(errPtr,"Error: bad ambFuncType in ddm_convolveFFT"); break;
    }
    ddm_ifft();
}

void ddm_convolveH_FFT(int ambFuncType){
    // convolves the working DDM with the ambiguity function.  Different
    // versions of the amb func are used for different purposes
    ddm_h_fft();
    switch (ambFuncType) {
        case 0:  for(int i = 0; i<ddm.numBins; i++) { H[i] = DDM_amb[i]  * H[i]; } break;
        case 1:  for(int i = 0; i<ddm.numBins; i++) { H[i] = DDM_amb1[i] * H[i]; } break;
        case 2:  for(int i = 0; i<ddm.numBins; i++) { H[i] = DDM_amb2[i] * H[i]; } break;
        default: fprintf(errPtr,"Error: bad ambFuncType in ddm_convolveHFFT"); break;
    }
    ddm_h_ifft();
}


void ddm_fft(void){  fftw_execute(ddm.FFTWPLAN); }

void ddm_ifft(void){ fftw_execute(ddm.IFFTWPLAN); ddm_scale( (1.0/ddm.numBins)); }

void ddm_h_fft(void){  fftw_execute(h.FFTWPLAN); }

void ddm_h_ifft(void){ fftw_execute(h.IFFTWPLAN); ddm_h_scale( (1.0/ddm.numBins)); }



void ddm_fftshift(void){
    circshift(DDM_temp, DDM , ddm.numDoppBins,  ddm.numDelayBins, (ddm.numDoppBins/2),  (ddm.numDelayBins/2));
    for(int i = 0; i<ddm.numBins; i++) { DDM[i] = DDM_temp[i];}
}

void circshift(DDMtype *out, const DDMtype *in, int xdim, int ydim, int xshift, int yshift){
    int ii,jj;
    for (int i =0; i < xdim; i++) {
        ii = (i + xshift) % xdim;
        for (int j = 0; j < ydim; j++) {
            jj = (j + yshift) % ydim;
            out[ii * ydim + jj] = in[i * ydim + j];
        }
    }
}

/****************************************************************************/
//  Ambiguity Function (analytic, unfiltered)
//  based on eqns (20) and (21) [Zavorotny & Voronovich 2000]
/****************************************************************************/

void ddm_initAmbFuncBuffers(void ) {
    // called from ddm_initialize

    // generate amb func and align it to first bin, store a copy in temp buffer
    ddm_genAmbFunc();
    ddm_fftshift();
    for(int i = 0; i<ddm.numBins; i++) {  DDM_temp[i] = DDM[i]; }

    // save FFT'd version in DDM_amb buffer
    ddm_fft();
    for(int i = 0; i<ddm.numBins; i++) {  DDM_amb[i] = DDM[i]; }

    // save a normalized, FFT'd version in DDM_amb1 buffer
    for(int i = 0; i<ddm.numBins; i++) {  DDM_amb1[i] = sqrt(cabs(DDM_amb[i])); }

    // take mag squared, FFT it, and save it in DDM_amb2 buffer
    for(int i = 0; i<ddm.numBins; i++) {  DDM[i] = DDM_temp[i]; }
    ddm_magSqr();
    ddm_fft();
    for(int i = 0; i<ddm.numBins; i++) {  DDM_amb2[i] = DDM[i]; }

    // reset DDM buffer to zero when we are done
    _ddm_zero( DDM, ddm.numBins );
}

void ddm_genAmbFunc(void){
    // generate (non-squared) amb func. centered in DDM buffer
    double dtau_s, dfreq_Hz;

    double cohIntTime_s  = ddm.cohIntegrationTime_s;  // 0.001
    double tauChip_s     = 1 / ddm.chipsPerSec; // 1/1.023e6
    int centerDelayBin   = (int) floor(ddm.numDelayBins/2); //200
    int centerDopplerBin = (int) floor(ddm.numDoppBins/2);  //200

    for (int l=0; l < ddm.numDelayBins; l++) {
        dtau_s =  (l - centerDelayBin) * ddm.delayRes_chips * tauChip_s;
        for (int k=0; k < ddm.numDoppBins; k++) {
            dfreq_Hz = (k - centerDopplerBin) * ddm.dopplerRes_Hz;
            DDM[DDMINDEX(k,l)] = lambda(dtau_s,tauChip_s,cohIntTime_s) * S(dfreq_Hz,cohIntTime_s);
        }
    }
}

double lambda( double dtau_s, double tauChip_s, double cohIntTime_s ) { //perfect triangle
    return (fabs(dtau_s) <= (tauChip_s*(1+tauChip_s/cohIntTime_s))) ?
           (1 - fabs(dtau_s)/tauChip_s) : -tauChip_s/cohIntTime_s;
}

complex double S(double dfreq_Hz, double cohIntTime_s) {
    double x       = dfreq_Hz*pi*cohIntTime_s;
    double ang_rad = -1*pi*dfreq_Hz*cohIntTime_s;
    return (x==0) ? 1 : (sin(x)/x) * (cos(ang_rad) + I*sin(ang_rad));
}


/****************************************************************************/
//  Thermal Noise
/****************************************************************************/

void ddm_initThermalNoise(struct metadata meta){
    // called from ddm_initialize

    // get thermal noise params from config file
    double k           = -228.599167840;	// Boltzman, dBW/K/Hz
    double temp_K      = meta.temp_K;
    double noiseFig_dB = meta.noiseFigure_dB;
    double Ti          = 0.001;
    double BW_Hz       = 1/Ti;

    double noisePower_abs =  pow(10,(k/10)) * temp_K * BW_Hz * pow(10,(noiseFig_dB/10));
    double noisePower_dBW = 10*log10(noisePower_abs);


    ddm.thermalNoisePwr_abs = sqrt(noisePower_abs);
}

void ddm_addGaussianNoise(void){
    // Add complex, Gaussian noise to DDM (colored with ambiguity function)
    // Currently, the convolution is done separately from the DDM/speckle convolution
    // due to the normalization.
    for(int i = 0; i<ddm.numBins; i++) {  DDM_temp[i] = DDM[i]; DDM[i] = 0;  }
    ddm_addWhiteGaussianNoise(ddm.thermalNoisePwr_abs);
    ddm_convolveFFT(1);
    for(int i = 0; i<ddm.numBins; i++) { DDM[i] += DDM_temp[i]; }
}

void ddm_addWhiteGaussianNoise(double sigma){
    // Add complex, *white* Gaussian noise to DDM (mean = 0 and variance = sigma^2)
#define RAND_GEN_TYPE 1
    double U1,U2;
    double R,T1,T2;

#if (RAND_GEN_TYPE == 1) // standard Box-Muller Method
    for(int i = 0; i<ddm.numBins; i++) {
        U1 = uniformRandf();
        U2 = uniformRandf();
        R  = sigma*sqrt(-2.0*log(U1))*(1/sqrt(2));
        T1 = R*cos(2*pi*U2);
        T2 = R*sin(2*pi*U2);
        DDM[i] += T1 + I*T2;
    }
#endif

#if (RAND_GEN_TYPE == 2) // polar form (supposedly faster ...)
    double S;
    for(int i = 0; i<ddm.numBins; i++) {
        do {
            U1=2 * uniformRandf() - 1; /* U1=[-1,1] */
            U2=2 * uniformRandf() - 1; /* U2=[-1,1] */
            S=U1 * U1 + U2 * U2;
        } while(S >= 1);
        R  = sigma*sqrt(-2.0*log(S) / S)*(1/sqrt(2));
        T1 = R*U1;
        T2 = R*U2;
        DDM[i] += T1 + I*T2;
    }
#endif
}

// uniform random number (0,1]. never = 0 so that we can safely take log of it
double uniformRandf( void ) { return ((double)rand() + 1.0)/((double)RAND_MAX + 1.0); }

void ddm_addRandomPhase(void){
    // adds a uniform random phase to the DDM
    // (for testing a poor man's speckle noise)
    for(int i = 0; i<ddm.numBins; i++) { DDM[i] *= cexp(I * uniformRandf() * 2 * pi); }
}

void testNoisePowerLevels(void){
    // used internally for debugging noise powers
    printf("Noise Power 1 is:  %f (dBW)\n", 20*log10(ddm.thermalNoisePwr_abs) );

    _ddm_zero( DDM,      ddm.numBins );
    ddm_addGaussianNoise();
    printf("Noise Power 2 is:  %f (dBW)\n", 20*log10(ddm_getRMS()) );

    ddm_resetRunningAvg();
    for( int i=0; i<100; i++){
        _ddm_zero( DDM,      ddm.numBins );
        ddm_addGaussianNoise();
        ddm_magSqr();
        ddm_addToRunningAvg();
    }
    ddm_getRunningAvg();
    printf("Noise Power 3 is:  %f (dBW)\n", 10*log10(ddm_getRMS()) );
}


/****************************************************************************/
//  Operations on DDMs:  These are various operations performed on the DDM.
//  Note that there are several DDM buffers.  "DDM" is the working DDM buffer
//  where most of the work happens, but there is "DDM_avg" for running averages
//  (i.e. non-coherent integration) & "DDM_store" for storing a DDM for later
//  use.
/****************************************************************************/

void ddm_normalize(void){
    double max = ddm_getMax();
    double min = ddm_getMin();
    if(max == 0) return;
    for (int i = 0; i < ddm.numBins; i++){
        DDM[i] = (cabs(DDM[i])-min)/(max-min);
    }
}

double ddm_getMax(void){
    double val, max = creal(DDM[0]);
    for (int i = 0; i < ddm.numBins; i++){
        val = creal(DDM[i]);
        if (val > max){ max = val;}
    }
    return max;
}

double ddm_getMin(void){
    double val, min = creal(DDM[0]);
    for (int i = 0; i < ddm.numBins; i++){
        val = creal(DDM[i]);
        if (val < min){ min = val;}
    }
    return min;
}

double ddm_getRMS(void)       { double val=0; for (int i = 0; i < ddm.numBins; i++){val += pow(cabs(DDM[i]),2);} return sqrt(val/ddm.numBins); }
double ddm_integrate(void)    { double val=0; for (int i = 0; i < ddm.numBins; i++){val += cabs(DDM[i]);} return val; }
void ddm_mag(void)            { for(int i = 0; i<ddm.numBins; i++) { DDM[i] = cabs(DDM[i]); } }
void ddm_angle(void)          { for(int i = 0; i<ddm.numBins; i++) { DDM[i] = carg(DDM[i]); } }
void ddm_real(void)           { for(int i = 0; i<ddm.numBins; i++) { DDM[i] = creal(DDM[i]); } }
void ddm_imag(void)           { for(int i = 0; i<ddm.numBins; i++) { DDM[i] = cimag(DDM[i]); } }
void ddm_magSqr(void)         { for(int i = 0; i<ddm.numBins; i++) { DDM[i] = pow(cabs(DDM[i]),2);} }
void ddm_sqrt(void)           { for(int i = 0; i<ddm.numBins; i++) { DDM[i] = csqrt(DDM[i]);} }
void ddm_convertTodB(void)    { for(int i = 0; i<ddm.numBins; i++) { DDM[i] = (DDM[i] == 0) ? ddm.refPower_dB : 10*log10(cabs(DDM[i])); } }
void ddm_scale(double c)      { for(int i = 0; i<ddm.numBins; i++) { DDM[i] *= c; } }
void ddm_h_scale(double c)      { for(int i = 0; i<ddm.numBins; i++) { H[i] *= c; } }
void ddm_store(void)          { for(int i = 0; i<ddm.numBins; i++) { DDM_store[i] = DDM[i]; } }
void ddm_restore(void)        { for(int i = 0; i<ddm.numBins; i++) { DDM[i] = DDM_store[i]; } }
void ddm_addToRunningAvg(void){ for(int i = 0; i<ddm.numBins; i++) { DDM_avg[i] = DDM_avg[i] + DDM[i]; } DDM_avgCount++; }
void ddm_getRunningAvg(void)  { for(int i = 0; i<ddm.numBins; i++) { DDM[i] = DDM_avg[i] / DDM_avgCount; } }
void ddm_resetRunningAvg(void){ DDM_avgCount = 0; _ddm_zero( DDM_avg,  ddm.numBins ); }
int  ddm_checkNAN(void)       { for(int i = 0; i<ddm.numBins; i++) { if( isnan(DDM[i]) ) return 1; } return 0; }

/****************************************************************************/
//  Save DDM to binary file or .png image
/****************************************************************************/

void ddm_save(struct metadata meta, struct DDMfm *ddm_fm, int realOrComplex){
    int startDelay_bin    = meta.resample_startBin[0];//0
    int startDoppelr_bin  = meta.resample_startBin[1];//100
    int resDelay_bins     = meta.resample_resolution_bins[0];//5
    int resDoppelr_bins   = meta.resample_resolution_bins[1];//20
    int numDelayBins      = meta.resample_numBins[0];//17
    int numDoppelrBins    = meta.resample_numBins[1];//11
    double val;
    complex double valc;

    int i = 0;
    switch(realOrComplex){  //case=1
        case 1: // real
            //for(int i = 0; i<ddm.numBins; i++) {
            //    val = creal(DDM[i]);
            //    fwrite(&val, 1, sizeof(double), outp);
            //}

            for (int k=startDoppelr_bin; k < (numDoppelrBins*resDoppelr_bins + startDoppelr_bin); k+=resDoppelr_bins) {
                for (int l=startDelay_bin; l < (numDelayBins*resDelay_bins + startDelay_bin); l+=resDelay_bins) {
                    ddm_fm->data[i].power = creal(DDM[DDMINDEX(k,l)]);
                    ddm_fm->data[i].delay = (l - meta.specular_delayBinIdx) * meta.delayRez_chips;
                    ddm_fm->data[i].Doppler = (k - meta.specular_dopplerBinIdx) * meta.dopplerRes_Hz;
                    i=i+1;
                }
            }
            break;

        case 2: // complex under construction
            for(int i = 0; i<ddm.numBins; i++) {
                valc = DDM[i];
            }
            break;
        default:
            fprintf(errPtr,"Error: bad realOrComplex in ddm_saveToFile");
            exit(0);
    }
}


double getImagePixelValue( int x, int y ){
    return creal(DDM[DDMINDEX(x, y)]);
}
/*
void ddm_save2PNG(const char* filename, int plotType, int limType, double min, double max ) {
    unsigned height = ddm.numDelayBins;
    unsigned width  = ddm.numDoppBins;
    unsigned char* image = malloc(width * height * 4);
    unsigned x, y;
    unsigned char color[3];
    double m1,m2;

    // save a copy in temp buffer
    for (int i = 0; i < ddm.numBins; i++){  DDM_temp[i] = DDM[i]; }

    // select component of DDM to plot
    switch (plotType){
        case 1: ddm_mag(); break;
        case 2: ddm_angle(); break;
        case 3: ddm_real(); break;
        case 4: ddm_imag(); break;
        case 5: ddm_convertTodB(); break;
        case 6:
            for(y = 0; y < height; y++)
                for(x = 0; x < width; x++)
                    DDM[DDMINDEX(x, y)] = x;
            break;
        case 7:
            for(y = 0; y < height; y++)
                for(x = 0; x < width; x++)
                    DDM[DDMINDEX(x, y)] = y;
            break;
        case 8: _ddm_zero( DDM, ddm.numBins ); break;
        default:
            fprintf(errPtr,"Error: Bad plotType in ddm_save2PNG");
            exit(0);
    }

    // select color limits to use (specified limits or auto limits)
    switch (limType) {
        case 1:  m1 = min; m2 = max; break;
        case 2:  m1 = ddm_getMin(); m2 = ddm_getMax(); break;
        default:  fprintf(errPtr,"Error: Bad limType in ddm_save2PNG"); exit(0);
    }

    // plot to png
    for(y = 0; y < height; y++)
        for(x = 0; x < width; x++) {
            image_mapFloat2RGB( m1, m2, getImagePixelValue(x,y), color );
            image[4 * width * y + 4 * x + 0] = color[0];
            image[4 * width * y + 4 * x + 1] = color[1];
            image[4 * width * y + 4 * x + 2] = color[2];
            image[4 * width * y + 4 * x + 3] = 255;
        }
    image_encodePNG(filename, image, width, height);
    free(image);

    // restore original DDM value from temp buffer
    for (int i = 0; i < ddm.numBins; i++){  DDM[i] = DDM_temp[i]; }
}
*/

/****************************************************************************/
// internal functions for alloc/dealloc DDM buffers
/****************************************************************************/

void _ddm_alloc( DDMtype **ptr, int numBins ) {
    *ptr = (DDMtype *) calloc( numBins, sizeof(DDMtype) );
    if (*ptr==NULL) {
        fprintf(errPtr,"Error allocating memory for DDM \n");
        exit (1);
    }
}

void _ddm_free( DDMtype *ptr ) {
    if (ptr!=NULL) {
        free(ptr);
    }
}

void _ddm_zero( DDMtype *ptr, int numBins  ) {
    if (ptr==NULL) {
        printf ("Error: Null pointer to _ddm_zero \n ");
        exit (1);
    }
    for(int i = 0; i<numBins; i++) {
        ptr[i] = 0 + I*0;
    }
}

