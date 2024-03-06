#include <sstream>
#include <fstream>
#include "config.h"
#include "catch.hpp"

#include "soilinput.h"
#include "commandlinearguments.h"

static const double TOLERANCE = 1e-10;

TEST_CASE("Soil input can be read using standard 0.5x0.5 degree grid starting at 0.25", "[soil][input]") {

    SoilInput soilinput = SoilInput();
    soilinput.soil_code = false;

    std::string soil_file_content("lon lat sand silt clay orgc bulkdensity ph soilc cn\n"
                 "9.25 47.25 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1");
    std::stringstream soil_file_content_stream (soil_file_content);

    soilinput.load_mineral_soils(soil_file_content_stream);

    SECTION( "When searching for exactly correct coordinate" ) {
        double lon = 9.25;
        double lat = 47.25;

        const coord &soil_data_coord = soilinput.find_closest_point_using_kd_tree(0.1, std::make_pair(lon, lat));

        REQUIRE(soil_data_coord.first == Approx(lon).margin(TOLERANCE));
        REQUIRE(soil_data_coord.second == Approx(lat).margin(TOLERANCE));
    }

    SECTION( "When searching for slightly different coordinate" ) {
        double lon = 9.2;
        double lat = 47.2;

        const coord &soil_data_coord = soilinput.find_closest_point_using_kd_tree(0.1, std::make_pair(lon, lat));

        REQUIRE(soil_data_coord.first == Approx(9.25).margin(TOLERANCE));
        REQUIRE(soil_data_coord.second == Approx(47.25).margin(TOLERANCE));
    }

    SECTION( "Fails when found soil point is too far away" ) {
        REQUIRE_THROWS_WITH(soilinput.find_closest_point_using_kd_tree(0.1, std::make_pair(123, 80)), Catch::Contains("No available soil data"));
    }

}



TEST_CASE("Soil input can be read using finer grid, with multiple entries", "[soil][input]") {

    SoilInput soilinput = SoilInput();
    soilinput.soil_code = false;

    std::string soil_file_content("lon lat sand silt clay orgc bulkdensity ph soilc cn\n"
                                  "9.125 47.125 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.125 47.25 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.125 47.375 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.125 47.5 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.25 47.125 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.25 47.25 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.25 47.375 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.25 47.5 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.375 47.125 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.375 47.25 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.375 47.375 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.375 47.5 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.5 47.125 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.5 47.25 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.5 47.375 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
                                  "9.5 47.5 45.0 36.0 19.0 1.0 1.41 6.4 4.23 -1\n"
    );
    std::stringstream soil_file_content_stream (soil_file_content);

    soilinput.load_mineral_soils(soil_file_content_stream);

    SECTION( "When searching for exactly correct coordinate" ) {
        double lon = 9.5;
        double lat = 47.375;

        const coord &soil_data_coord = soilinput.find_closest_point_using_kd_tree(0.1, std::make_pair(lon, lat));

        REQUIRE(soil_data_coord.first == Approx(lon).margin(TOLERANCE));
        REQUIRE(soil_data_coord.second == Approx(lat).margin(TOLERANCE));
    }

    SECTION( "When searching for slightly different coordinate" ) {
        double lon = 9.12;
        double lat = 47.378;

        const coord &soil_data_coord = soilinput.find_closest_point_using_kd_tree(0.1, std::make_pair(lon, lat));

        REQUIRE(soil_data_coord.first == Approx(9.125).margin(TOLERANCE));
        REQUIRE(soil_data_coord.second == Approx(47.375).margin(TOLERANCE));
    }

    SECTION( "Fails when found soil point is too far away" ) {
        REQUIRE_THROWS_WITH(soilinput.find_closest_point_using_kd_tree(0.1, std::make_pair(123, 80)), Catch::Contains("No available soil data"));
    }

}

TEST_CASE("Proper error message when input file not found (and not just a SegFault)", "[soil][input][error]"){

    SoilInput soilinput = SoilInput();
    soilinput.soil_code = false;

    std::vector<coord> gridlist(0);
    gridlist.emplace_back(coord(1, 2));
    std::set<coord> coords(gridlist.begin(), gridlist.end());

    const char* fname = "/file/that/does/not/exist.csv";
    REQUIRE_THROWS_WITH(soilinput.load_mineral_soils(fname, coords), Catch::Contains("Soil data file not found!"));
}


TEST_CASE("Soil input can be read with full HWSD data", "[soil][input]") {

    SoilInput soilinput = SoilInput();
    soilinput.soil_code = false;

    std::ifstream soil_file_content_stream ("/home/konni/Documents/konni/projekte/phd/bavariaopt/inputs/hwsd2_nonnan.csv");
    auto start = std::chrono::high_resolution_clock::now();
    soilinput.load_mineral_soils(soil_file_content_stream);
    auto finish = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = finish - start;
    dprintf("Reading soil data took %.3f seconds\n", elapsed.count());

    double lon = 9.5;
    double lat = 47.1;

    coord soil_data_coord = soilinput.find_closest_point_using_kd_tree(1, std::make_pair(lon, lat));
    auto finish_read = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed2 = finish_read - finish;
    dprintf("Found soil data point in %.3f seconds\n", elapsed2.count());
    REQUIRE(soil_data_coord.first == Approx(9.475).margin(TOLERANCE));
    REQUIRE(soil_data_coord.second == Approx(47.125).margin(TOLERANCE));


    finish = std::chrono::high_resolution_clock::now();
    soil_data_coord = soilinput.find_closest_point_using_kd_tree(1, std::make_pair(123, 16));
    finish_read = std::chrono::high_resolution_clock::now();
    elapsed2 = finish_read - finish;
    dprintf("Found soil data point in %.3f seconds\n", elapsed2.count());
    REQUIRE(soil_data_coord.first == Approx(122.225).margin(TOLERANCE));
    REQUIRE(soil_data_coord.second == Approx(16.325).margin(TOLERANCE));

    // does not find data for this point in South Atlantic Ocean
    REQUIRE_THROWS_WITH(soilinput.find_closest_point_using_kd_tree(0.1, std::make_pair(-23, -46)), Catch::Contains("No available soil data"));

    // but with extremely high search radius, of course we find something.
    finish = std::chrono::high_resolution_clock::now();
    soil_data_coord = soilinput.find_closest_point_using_kd_tree(1000, std::make_pair(-23, -46));
    finish_read = std::chrono::high_resolution_clock::now();
    elapsed2 = finish_read - finish;
    dprintf("Found soil data point in %.3f seconds\n", elapsed2.count());
}