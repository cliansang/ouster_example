#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "helpers.h"
#include "ouster/build.h"
#include "ouster/client.h"
#include "ouster/lidar_scan.h"
#include "ouster/types.h"

using namespace ouster;

//! [docs-stag-x-image-form]
img_t<double> get_x_in_image_form(const LidarScan& scan, bool destaggered,
                                  const sensor::sensor_info& info) {
    // For convenience, save w and h to variables
    const size_t w = info.format.columns_per_frame;
    const size_t h = info.format.pixels_per_column;

    // Get the XYZ in ouster::Points (n x 3 Eigen array) form
    XYZLut lut = make_xyz_lut(info);
    auto cloud = cartesian(scan.field(sensor::ChanField::RANGE), lut);

    // Access x and reshape as needed
    // Note that the values in cloud.col(0) are ordered
    auto x = Eigen::Map<const img_t<double>>(cloud.col(0).data(), h, w);
    auto x_destaggered = destagger<double>(x, info.format.pixel_shift_by_row);

    // Apply destagger if desired
    if (!destaggered) return x;
    return x_destaggered;
}
//! [docs-etag-x-image-form]
//

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "\n\nUsage: lidar_scan_example <pcap_file> <json_file>"
                  << std::endl;
        return EXIT_FAILURE;
    }

    const std::string pcap_file = argv[1];
    const std::string json_file = argv[2];

    auto handle = sensor_utils::replay_initialize(pcap_file);
    auto info = sensor::metadata_from_json(json_file);

    size_t w = info.format.columns_per_frame;
    size_t h = info.format.pixels_per_column;

    auto scan = LidarScan(w, h);

    std::cerr << "Reading in scan from pcap..." << std::endl;
    get_complete_scan(handle, scan, info);

    // 1. Getting XYZ
    //! [doc-stag-cpp-xyz]
    std::cerr << "1. Calculating 3d Points... " << std::endl;
    XYZLut lut = make_xyz_lut(info);
    auto range = scan.field(sensor::ChanField::RANGE);
    auto cloud = cartesian(range, lut);
    //! [doc-etag-cpp-xyz]
    //
    std::cerr << "\nLet's see what the 2000th point in this cloud is...  ("
              << cloud(2000, 0) << ", " << cloud(2000, 1) << ", "
              << cloud(2000, 2) << ")" << std::endl;

    // 2. Providing a transfomration to XYZ
    // You can also make an XYZLut by specifying a special transform if you
    // have a different frame you would like to be in, say if you have an
    // extrinsics matrix:
    Eigen::Matrix<double, 4, 4, Eigen::DontAlign> transformation =
        mat4d::Identity();

    // Let's turn it upside down and put it on a very tall pole and shift x
    transformation(2, 2) = -1;
    transformation(1, 1) = -1;
    transformation(2, 3) = 20000;  // unit is mm, so taht's 20 meters
    transformation(0, 3) = 1500;   // unit is mm

    transformation = transformation * info.lidar_to_sensor_transform;
    std::cerr
        << "2. Now we will apply this transformation to the look-up table:\n"
        << transformation << std::endl;
    // Remember to apply the lidar_to_sensor_transform if your extrinsics
    // matrix was between sensor coordinate system and some stable point,
    // say a vehicle center
    // TODO -- double check that this order of matrix application is correct
    auto lut_extrinsics = make_xyz_lut(
        w, h, sensor::range_unit, info.lidar_origin_to_beam_origin_mm,
        transformation, info.beam_azimuth_angles, info.beam_altitude_angles);

    std::cerr << "Calculating 3d Points of with special transform provided.."
              << std::endl;
    auto cloud_adjusted = cartesian(range, lut_extrinsics);

    std::cerr << "And now the 2000th point in the transformed point cloud... ("
              << cloud_adjusted(2000, 0) << ", " << cloud_adjusted(2000, 1)
              << ", " << cloud_adjusted(2000, 2) << ")" << std::endl;

    // 3. Destaggering
    // Fields come in w x h arrays, but they are staggered, so that a column
    // reflects the timestamp. To get each column to make visual sense,
    // destagger the image
    std::cerr
        << "\n3. Getting staggered and destaggered images of Reflectivity..."
        << std::endl;
    auto reflectivity = scan.field(sensor::ChanField::REFLECTIVITY);
    auto reflectivity_destaggered =
        destagger<uint32_t>(reflectivity, info.format.pixel_shift_by_row);

    // 4. You can get XYZ in w x h arrays too
    std::cerr
        << "4. Getting staggered and destaggered images of X Coordinate..."
        << std::endl;
    auto x_image_staggered = get_x_in_image_form(scan, false, info);
    auto x_image_destaggered = get_x_in_image_form(scan, true, info);

    const auto print_row = std::min<size_t>(123, h - 3);
    const auto print_column = std::min<size_t>(1507, w / 2 + 5);

    const std::string point_string = "(" + std::to_string(print_row) + ", " +
                                     std::to_string(print_column) + ")";

    std::cerr << "In the staggered image, the point at " << point_string
              << " has reflectivity " << reflectivity(print_row, print_row)
              << " and an x coordinate of "
              << x_image_staggered(print_column, print_column) << "."
              << std::endl;
    std::cerr << "In the destagged image, the point at " << point_string
              << " has reflectivity "
              << reflectivity_destaggered(print_row, print_column)
              << " and an x coordinate of "
              << x_image_destaggered(print_row, print_column) << "."
              << std::endl;
}
