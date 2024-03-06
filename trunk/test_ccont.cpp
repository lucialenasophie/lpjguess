
#include "config.h"
#include "commonoutput.h"
#include "parameters.h"
#include "guess.h"

void GuessOutput::CommonOutput::outannual(Gridcell& gridcell) {
    int m;
    double mlai[12];

    double lon = gridcell.get_lon();
    double lat = gridcell.get_lat();

    // The OutputRows object manages the next row of output for each
    // output table
    OutputRows out(output_channel, lon, lat, date.get_calendar_year());

    // guess2008 - reset monthly and annual sums across patches each year
    for (m = 0; m < 12; m++) {
       mlai[m] = 0.0;
    }

    double landcover_lai[NLANDCOVERTYPES]={0.0};
    double landcover_densindiv_total[NLANDCOVERTYPES]={0.0};

    double mean_standpft_lai=0.0;
    double mean_standpft_densindiv_total=0.0;

    double lai_gridcell=0.0;
    double dens_gridcell=0.0;

    double standpft_lai=0.0;
    double standpft_densindiv_total=0.0;

    // *** Loop through PFTs ***

    pftlist.firstobj();
    while (pftlist.isobj) {

        Pft& pft=pftlist.getobj();
        Gridcellpft& gridcellpft=gridcell.pft[pft.id];

        // Sum C biomass, NPP, LAI and BVOC fluxes across patches and PFTs
        mean_standpft_lai=0.0;
        mean_standpft_densindiv_total=0.0;

        Gridcell::iterator gc_itr = gridcell.begin();

        // Loop through Stands
        gc_itr = gridcell.begin();

        while (gc_itr != gridcell.end()) {
            Stand& stand = *gc_itr;

            Standpft& standpft=stand.pft[pft.id];
            if(standpft.active) {
                // Sum C biomass, NPP, LAI and BVOC fluxes across patches and PFTs

                standpft_lai=0.0;
                standpft_densindiv_total = 0.0;


                stand.firstobj();

                // Loop through Patches
                while (stand.isobj) {
                    Patch& patch = stand.getobj();
                    Patchpft& patchpft = patch.pft[pft.id];
                    Vegetation& vegetation = patch.vegetation;


                    vegetation.firstobj();
                    while (vegetation.isobj) {
                        Individual& indiv=vegetation.getobj();

                        if (indiv.id!=-1 && indiv.alive) {

                            if (indiv.pft.id==pft.id) {

                                standpft_lai += indiv.lai;
                                if (pft.lifeform==TREE) {
                                    standpft_densindiv_total += indiv.densindiv;
                                }
                            }

                        } // alive?
                        vegetation.nextobj();
                    }

                    stand.nextobj();
                } // end of patch loop

                standpft_lai/=(double)stand.npatch();
                standpft_densindiv_total/=(double)stand.npatch();


                //Update landcover totals

                landcover_densindiv_total[stand.landcover]+=standpft_densindiv_total*stand.get_landcover_fraction();

                // Update gridcell totals
                double fraction_of_gridcell = stand.get_gridcell_fraction();
                if(!pft.isintercropgrass) {
                    lai_gridcell+=standpft_lai*fraction_of_gridcell;
                }
                dens_gridcell+=standpft_densindiv_total*fraction_of_gridcell;


            }//if(active)
            ++gc_itr;
        }//End of loop through stands

        pftlist.nextobj();

    } // *** End of PFT loop ***

    // Print monthly output variables
    for (m=0;m<12;m++) {
        outlimit(out,out_mlai,         mlai[m]);
        outlimit(out,out_mburned_area, (float)gridcell.monthly_burned_area[m]);
    }

    // Write fluxes to file

    Landcover& lc = gridcell.landcover;

    // CPOOL Write cpool to file


}