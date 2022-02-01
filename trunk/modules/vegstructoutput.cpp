///////////////////////////////////////////////////////////////////////////////////////
/// \file vegstructoutput.cpp
/// \brief Output module for patch based vegetation structure
///
/// \author Joerg Steinkamp
/// $Date: Thu Nov  3 11:15:37 CET 2016 $
///
///////////////////////////////////////////////////////////////////////////////////////
#include "config.h"
#include "vegstructoutput.h"
#include "parameters.h"
#include "guess.h"
#include <iostream>

namespace GuessOutput {
    REGISTER_OUTPUT_MODULE("vegstruct", VegstructOutput)

    VegstructOutput::VegstructOutput() {
        declare_parameter("file_vegstruct", &file_vegstruct, 300, "Detailed vegetation structure");
    }

    VegstructOutput::~VegstructOutput() {
    }

    void VegstructOutput::init() {
        if (file_vegstruct != "") {
            std::string full_path =  (char*) file_vegstruct;
            std::string head_path{(char*) path_vegstruct};
            full_path = head_path + full_path;
            out_vegstruct = fopen(full_path.c_str(), "w");
            if (out_vegstruct == NULL) {
                fail("Could not open %s for output\n"                         \
             "Close the file if it is open in another application",
                     full_path.c_str());
            } else {
                dprintf("dummy\n");
                fprintf(out_vegstruct, "Lon Lat Year SID PID VID Pft Lifeform LeafType PhenType Pathway Age LAI ccont ShadeType N DBH Height Crownarea\n");
            }
        }
    }

    void VegstructOutput::outdaily(Gridcell& gridcell) {
        return;
    }

    void VegstructOutput::outannual(Gridcell& gridcell) {
        if (file_vegstruct == "")
            return;
        if (date.year >= nyear_spinup-50) {
            double lon = gridcell.get_lon();
            double lat = gridcell.get_lat();
            Gridcell::iterator gc_itr = gridcell.begin();
            while (gc_itr != gridcell.end()) {
                Stand& stand = *gc_itr;
                stand.firstobj();
                while (stand.isobj) {
                    Patch& patch = stand.getobj();
                    Vegetation& vegetation = patch.vegetation;
                    vegetation.firstobj();
                    while (vegetation.isobj) {
                        Individual& indiv=vegetation.getobj();
                        // guess2008 - alive check added
                        if (indiv.id != -1 && indiv.alive) {
                            fprintf(out_vegstruct, "%7.2f %6.2f %4i ", lon, lat, date.get_calendar_year() );
                            fprintf(out_vegstruct, " %i ",    stand.id);
                            fprintf(out_vegstruct, " %i ",    patch.id);
                            fprintf(out_vegstruct, " %i ",    indiv.id);
                            fprintf(out_vegstruct, " %10s ",  (char*) indiv.pft.name);
                            fprintf(out_vegstruct, " %i ",    indiv.pft.lifeform);
                            fprintf(out_vegstruct, " %i ",    indiv.pft.leafphysiognomy);
                            fprintf(out_vegstruct, " %i ",    indiv.pft.phenology);
                            fprintf(out_vegstruct, " %i ",    indiv.pft.pathway);
                            fprintf(out_vegstruct, " %4.0f ", indiv.age);
                            fprintf(out_vegstruct, " %6.2f ", indiv.lai);
                            fprintf( out_vegstruct, " %6.2f ", indiv.ccont());
                            if (indiv.pft.lifeform == TREE) {
                                fprintf(out_vegstruct, " %4.1f ", indiv.pft.alphar);
                                fprintf(out_vegstruct, " %4.0f ", indiv.densindiv * patcharea);
                                fprintf(out_vegstruct, " %7.2f ", pow(indiv.height/indiv.pft.k_allom2,1.0/indiv.pft.k_allom3));
                                fprintf(out_vegstruct, " %8.2f ", indiv.height);
                                fprintf(out_vegstruct, " %8.2f ", indiv.crownarea);
                            } else if (indiv.pft.lifeform == GRASS) {
                                fprintf(out_vegstruct, " %4.1f ", -1.0);
                                fprintf(out_vegstruct, " %i ",     1);
                                fprintf(out_vegstruct, " %i ",    -1);
                                fprintf(out_vegstruct, " %i ",    -1);
                                fprintf(out_vegstruct, " %i ",    -1);
                            }
                            fprintf(out_vegstruct, "\n");
                        }
                        vegetation.nextobj();
                    }
                    stand.nextobj();
                }
                ++gc_itr;
            }
        }
    } // END of void VegStructOutput::outannual


    REGISTER_OUTPUT_MODULE("vegstruct_patch", VegstructOutputPatch)

    VegstructOutputPatch::VegstructOutputPatch() {
        declare_parameter("file_vegstruct_patch", &file_vegstruct_patch, 300, "Detailed vegetation structure on patch level");
    }
    VegstructOutputPatch::~VegstructOutputPatch() {
    }

    void VegstructOutputPatch::init() {
        if (file_vegstruct_patch != "") {
            std::string full_path =  (char*) file_vegstruct_patch;
            std::string head_path{(char*) path_vegstruct};
            full_path = head_path + full_path;
            out_vegstruct_patch = fopen(full_path.c_str(), "w");
            if (out_vegstruct_patch == NULL) {
                fail("Could not open %s for output\n"                         \
             "Close the file if it is open in another application",
                     full_path.c_str());
            } else {
                dprintf("dummy\n");
                fprintf(out_vegstruct_patch, "Lon Lat Year SID PID VID PFT ccont_total ccont_indiv ccont_patch_before ccont_path_after \n");
            }
        }
    }

    void VegstructOutputPatch::outdaily(Gridcell& gridcell) {
        return;
    }

    void VegstructOutputPatch::outannual(Gridcell &gridcell) {
        if (file_vegstruct_patch == "")
            return;
        if (date.year >= nyear_spinup-50) {
            double lon = gridcell.get_lon();
            double lat = gridcell.get_lat();
            // *** Loop through PFTs (per cohort) ***

            pftlist.firstobj();
            while (pftlist.isobj) {

                Pft &pft = pftlist.getobj();
                Gridcellpft &gridcellpft = gridcell.pft[pft.id];

                Gridcell::iterator gc_itr = gridcell.begin();
                while (gc_itr != gridcell.end()) {
                    Stand &stand = *gc_itr;
                    stand.firstobj();
                    while (stand.isobj) {
                        Patch &patch = stand.getobj();
                        Patchpft& patchpft = patch.pft[pft.id];
                        double patchpftccont{0};
                        //std::cout << "\ninitialized patchpftcont for pft=" << (char*) patchpft.pft.name << " and ";
                        //std::cout << " patch=" << patch.id;
                        Vegetation& vegetation = patch.vegetation;
                        vegetation.firstobj();
                        while (vegetation.isobj) {
                            Individual& indiv=vegetation.getobj();
                            // guess2008 - alive check added
                            if (indiv.id != -1 && indiv.alive) {
                                fprintf(out_vegstruct_patch, "%7.2f %6.2f %4i ", lon, lat, date.get_calendar_year());
                                fprintf(out_vegstruct_patch, " %i ", stand.id);
                                fprintf(out_vegstruct_patch, " %i ", patch.id);
                                fprintf(out_vegstruct_patch, " %i ", indiv.id);
                                fprintf(out_vegstruct_patch, " %10s ", (char*) patchpft.pft.name);
                                fprintf(out_vegstruct_patch, " %6.2f ", patch.ccont());
                                fprintf(out_vegstruct_patch, " %6.2f ", indiv.ccont());
                                fprintf(out_vegstruct_patch, "%6.2f", patchpftccont);
                                patchpftccont += indiv.ccont();
                                fprintf(out_vegstruct_patch, "%6.2f", patchpftccont);
                                fprintf(out_vegstruct_patch, "\n");

                            }
                            vegetation.nextobj();
                        }


                        stand.nextobj();
                    }
                    ++gc_itr;
                }
            pftlist.nextobj();
            }
        }

    }

} // END of namespace VegStructOutput
