/**
 * @file
 * @brief Point cloud and image visualizer for Ouster Lidar using OpenGL
 */
#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ouster {
namespace viz {

using mat4d = std::array<double, 16>;
constexpr mat4d identity4d = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

using vec4f = std::array<float, 4>;
using vec3d = std::array<double, 3>;

// TODO: messes up lidar_scan_viz
namespace impl {
class GLCloud;
class GLImage;
class GLCuboid;
class GLLabel;
class GLRings;
struct CameraData;
}  // namespace impl

struct WindowCtx;
class Camera;
class Cloud;
class Image;
class Cuboid;
class Label;
class TargetDisplay;

constexpr int default_window_width = 800;
constexpr int default_window_height = 600;

/**
 * @brief A basic visualizer for sensor data
 *
 * Displays a set of point clouds, images, cuboids, and text labels with a few
 * options for coloring and handling input.
 *
 * All operations are thread safe when running rendering (run() or run_once())
 * in a separate thread. This is the intended way to use the visualizer library
 * when a nontrivial amount of processing needs to run concurrently with
 * rendering (e.g. when streaming data from a running sensor).
 */
class PointViz {
   public:
    struct Impl;

    /**
     * Creates a window and initializes the rendering context
     *
     * @param name name of the visualizer, shown in the title bar
     */
    PointViz(const std::string& name, bool fix_aspect = false,
             int window_width = default_window_width,
             int window_height = default_window_height);

    /**
     * Tears down the rendering context and closes the viz window
     */
    ~PointViz();

    /**
     * Main drawing loop, keeps drawing things until running(false)
     *
     * Should be called from the main thread for macos compatibility
     */
    void run();

    /**
     * Run one iteration of the main loop for rendering and input handling
     *
     * Should be called from the main thread for macos compatibility
     */
    void run_once();

    /**
     * Check if the run() has been signaled to exit
     *
     * @return true if the run() loop is currently executing
     */
    bool running();

    /**
     * Set the running flag. Will signal run() to exit
     *
     * @param state new value of the flag
     */
    void running(bool state);

    /**
     * Show or hide the visualizer window
     *
     * @param state true to show
     */
    void visible(bool state);

    /**
     * Update visualization state
     *
     * Send state updates to be rendered on the next frame
     *
     * @return whether state was successfully sent. If not, will be sent on next
     *         call to update(). This can happen if update() is called more
     *         frequently than the frame rate.
     */
    bool update();

    /**
     * Add a callback for handling keyboard input
     *
     * @param f the callback. The second argument is the ascii value of the key
     *        pressed. Third argument is a bitmask of the modifier keys
     */
    void push_key_handler(std::function<bool(const WindowCtx&, int, int)>&& f);

    /**
     * Add a callback for handling mouse button input
     */
    void push_mouse_button_handler(
        std::function<bool(const WindowCtx&, int, int)>&& f);

    /**
     * Add a callback for handling mouse scrolling input
     */
    void push_scroll_handler(
        std::function<bool(const WindowCtx&, double, double)>&& f);

    /**
     * Add a callback for handling mouse movement
     */
    void push_mouse_pos_handler(
        std::function<bool(const WindowCtx&, double, double)>&& f);

    /**
     * Remove the last added callback for handling keyboard input
     */
    void pop_key_handler();
    void pop_mouse_button_handler();
    void pop_scroll_handler();
    void pop_mouse_pos_handler();

    /**
     * Get a reference to the camera controls
     */
    Camera& camera();

    /**
     * Get a reference to the target display controls
     */
    TargetDisplay& target_display();

    /**
     * Add an object to the scene
     */
    void add(const std::shared_ptr<Cloud>& cloud);
    void add(const std::shared_ptr<Image>& image);
    void add(const std::shared_ptr<Cuboid>& cuboid);
    void add(const std::shared_ptr<Label>& label);

    /**
     * Remove an object from the scene
     */
    bool remove(const std::shared_ptr<Cloud>& cloud);
    bool remove(const std::shared_ptr<Image>& image);
    bool remove(const std::shared_ptr<Cuboid>& cuboid);
    bool remove(const std::shared_ptr<Label>& label);

   private:
    std::unique_ptr<Impl> pimpl;
    void draw();
};

/**
 * Add default keyboard and mouse bindings to a visualizer instance
 *
 * Controls will modify the camera from the thread that calls run() or
 * run_once(), which will require synchronization when using multiple threads.
 *
 * @param viz the visualizer instance
 * @param mx mutex to lock while modifying camera
 */
void add_default_controls(viz::PointViz& viz, std::mutex* mx = nullptr);

/**
 * @brief Context for input callbacks
 */
struct WindowCtx {
    bool lbutton_down{false};  ///< True if the left mouse button is held
    bool mbutton_down{false};  ///< True of the middle mouse button is held
    double mouse_x{0};         ///< Current mouse x position
    double mouse_y{0};         ///< Current mouse y position
    int viewport_width{0};     ///< Current viewport width in pixels
    int viewport_height{0};    ///< Current viewport height in pixels
};

/**
 * @brief Controls the camera view and projection
 */
class Camera {
    /* view parameters */
    mat4d target_;
    vec3d view_offset_;
    int yaw_;  // decidegrees
    int pitch_;
    int log_distance_;  // 0 means 50m

    /* projection parameters */
    bool orthographic_;
    int fov_;
    double proj_offset_x_, proj_offset_y_;

    double view_distance() const;

   public:
    Camera();

    impl::CameraData matrices(double aspect) const;

    /**
     * Reset the camera view and fov
     */
    void reset();

    /**
     * Orbit the camera left or right about the camera target
     *
     * @param degrees offset to the current yaw angle
     */
    void yaw(float degrees);

    /**
     * Pitch the camera up or down
     *
     * @param degrees offset to the current pitch angle
     */
    void pitch(float degrees);

    /**
     * Move the camera towards or away from the target
     *
     * @param amount offset to the current camera distance from the target
     */
    void dolly(int amount);

    /**
     * Move the camera in the XY plane of the camera view
     *
     * Coordinates are normalized so that 1 is the length of the diagonal of the
     * view plane at the target. This is useful for implementing controls that
     * work intuitively regardless of the camera distance.
     *
     * @param x horizontal offset
     * @param y vertical offset
     */
    void dolly_xy(double x, double y);

    /**
     * Set the diagonal field of view
     *
     * @param degrees the diagonal field of view, in degrees
     */
    void set_fov(float degrees);

    /**
     * Use an orthographic or perspective projection
     *
     * @param state true for orthographic, false for perspective
     */
    void set_orthographic(bool state);

    /**
     * Set the 2d position of camera target in the viewport
     *
     * @param x horizontal position in in normalized coordinates [-1, 1]
     * @param y vertical position in in normalized coordinates [-1, 1]
     */
    void set_proj_offset(float x, float y);
};

/**
 * @brief Manages the state of the camera target display
 */
class TargetDisplay {
    int ring_size_{1};
    bool rings_enabled_{false};

   public:
    /**
     * Enable or disable distance ring display
     *
     * @param state true to display rings
     */
    void enable_rings(bool state);

    /**
     * Set the distance between rings
     *
     * @param n space between rings will be 10^n meters
     */
    void set_ring_size(int n);

    friend class impl::GLRings;
};

/**
 * @brief Manages the state of a point cloud
 *
 * Each point cloud consists of n points with w poses. The ith point will be
 * transformed by the (i % w)th pose. For example for 2048 x 64 Ouster lidar
 * point cloud, we may have w = 2048 poses and n = 2048 * 64 = 131072 points.
 *
 * We also keep track of a per-cloud pose to efficiently transform the
 * whole point cloud without having to update all ~2048 poses.
 */
class Cloud {
    size_t n_{0};
    size_t w_{0};
    mat4d extrinsic_{};

    bool range_changed_{false};
    bool key_changed_{false};
    bool mask_changed_{false};
    bool xyz_changed_{false};
    bool offset_changed_{false};
    bool transform_changed_{false};
    bool palette_changed_{false};
    bool pose_changed_{false};
    bool point_size_changed_{false};

    std::vector<float> range_data_{};
    std::vector<float> key_data_{};
    std::vector<float> mask_data_{};
    std::vector<float> xyz_data_{};
    std::vector<float> off_data_{};
    std::vector<float> transform_data_{};
    std::vector<float> palette_data_{};
    mat4d pose_{};
    float point_size_{2};

    Cloud(size_t w, size_t h, const mat4d& extrinsic);

   public:
    /**
     * Unstructured point cloud for visualization
     *
     * Call set_xyz() to update
     *
     * @param n number of points
     * @param extrinsic sensor extrinsic calibration. 4x4 column-major
     *        homogeneous transformation matrix
     */
    Cloud(size_t n, const mat4d& extrinsic = identity4d);

    /**
     * Structured point cloud for visualization
     *
     * Call set_range() to update
     *
     * @param w number of columns
     * @param h number of pixels per column
     * @param dir unit vectors for projection
     * @param off offsets for xyz projection
     * @param extrinsic sensor extrinsic calibration. 4x4 column-major
     *        homogeneous transformation matrix
     */
    Cloud(size_t w, size_t h, const float* dir, const float* off,
          const mat4d& extrinsic = identity4d);

    /**
     * Clear dirty flags
     *
     * Resets any changes since the last call to PointViz::update()
     */
    void clear();

    /**
     * Get the size of the point cloud
     */
    size_t get_size() { return n_; }

    /**
     * Set the range values
     *
     * @param x pointer to array of at least as many elements as there are
     *        points, representing the range of the points
     */
    void set_range(const uint32_t* range);

    /**
     * Set the key values, used for colouring.
     *
     * @param key pointer to array of at least as many elements as there are
     *        points, preferably normalized between 0 and 1
     */
    void set_key(const float* key);

    /**
     * Set the RGBA mask values, used as an overlay on top of the key
     *
     * @param mask pointer to array of at least 4x as many elements as there are
     *        points, preferably normalized between 0 and 1
     */
    void set_mask(const float* mask);

    /**
     * Set the XYZ values
     *
     * @param xyz pointer to array of exactly 3n where n is number of points, so
     *        that the xyz position of the ith point is i, i + n, i + 2n
     */
    void set_xyz(const float* xyz);

    /**
     * Set the offset values
     *
     * TODO: no real reason to have this. Set in constructor, if at all
     *
     * @param off pointer to array of exactly 3n where n is number of points, so
     *        that the xyz position of the ith point is i, i + n, i + 2n
     */
    void set_offset(const float* offset);

    /**
     * Set the ith point cloud pose
     *
     * @param pose 4x4 column-major homogeneous transformation matrix
     */
    void set_pose(const mat4d& pose);

    /**
     * Set point size
     */
    void set_point_size(float size);

    /**
     * Set the per-column poses, so that the point corresponding to the
     * pixel at row u, column v in the staggered lidar scan is transformed
     * by the vth pose, given as a homogeneous transformation matrix.
     *
     * @param rotation array of rotation matrices, total size 9 * w, where the
     *        vth rotation matrix is: r[v], r[w + v], r[2 * w + v], r[3 * w +
     *        v], r[4 * w + v], r[5 * w + v], r[6 * w + v], r[7 * w + v], r[8 *
     *        w + v]
     * @param translation translation vector array, column major, where each row
     *        is a translation vector. That is, the vth translation is t[v],
     *        t[w + v], t[2 * w + v]
     */
    void set_column_poses(const float* rotation, const float* translation);

    /**
     * Set the point cloud color palette
     *
     * @param palette the new palette to use, must have size 3*palette_size
     * @param palette_size the number of colors in the new palette
     */
    void set_palette(const float* palette, size_t palette_size);

    friend class impl::GLCloud;
};

/**
 * @brief Manages the state of an image
 */
class Image {
    bool position_changed_{false};
    bool image_changed_{false};
    bool mask_changed_{false};

    vec4f position_{};
    size_t image_width_{0};
    size_t image_height_{0};
    std::vector<float> image_data_{};
    size_t mask_width_{0};
    size_t mask_height_{0};
    std::vector<float> mask_data_{};

   public:
    Image();

    /**
     * Clear dirty flags
     *
     * Resets any changes since the last call to PointViz::update()
     */
    void clear();

    /**
     * Set the image data
     *
     * @param width width of the image data in pixels
     * @param height height of the image data in pixels
     * @param image_data pointer to an array of width * height elements
     *        interpreted as a row-major monochrome image
     */
    void set_image(size_t width, size_t height, const float* image_data);

    /**
     * Set the RGBA mask
     *
     * Not required to be the same resolution as the image data
     *
     * @param width width of the image data in pixels
     * @param height height of the image data in pixels
     * @param mask pointer to array of 4 * width * height elements interpreted
     *        as a row-major rgba image
     */
    void set_mask(size_t width, size_t height, const float* mask_data);

    /**
     * Set the display position of the image
     *
     * TODO: this is super weird. Coordinates are {x_min, x_max, y_max, y_min}
     * in sort-of normalized screen coordinates: y is in [-1, 1], and x uses the
     * same scale (i.e. window width is ignored). This is currently just the
     * same method the previous hard-coded 'image_frac' logic was using; I
     * believe it was done this way to allow scaling with the window while
     * maintaining the aspect ratio.
     *
     * @param pos the position of the image
     */
    void set_position(float x_min, float x_max, float y_min, float y_max);

    friend class impl::GLImage;
};

/**
 * @brief Manages the state of a single cuboid
 */
class Cuboid {
    bool transform_changed_{false};
    bool rgba_changed_{false};

    mat4d transform_{};
    vec4f rgba_{};

   public:
    Cuboid(const mat4d& transform, const vec4f& rgba);

    /**
     * Clear dirty flags
     *
     * Resets any changes since the last call to PointViz::update()
     */
    void clear();

    /**
     * Set the transform defining the cuboid
     *
     * Applied to a unit cube centered at the origin
     */
    void set_transform(const mat4d& pose);

    /**
     * Set the color of the cuboid
     */
    void set_rgba(const vec4f& rgba);

    friend class impl::GLCuboid;
};

/**
 * @brief Manages the state of a text label
 */
class Label {
    bool pos_changed_{false};
    bool scale_changed_{true};
    bool text_changed_{false};
    bool is_3d_{false};
    bool align_right_{false};

    vec3d position_{};
    float scale_{1.0};
    std::string text_{};

   public:
    Label(const std::string& text, const vec3d& position);
    Label(const std::string& text, float x, float y, bool align_right = false);

    /**
     * Clear dirty flags
     *
     * Resets any changes since the last call to PointViz::update()
     */
    void clear();

    /**
     * Update label text
     *
     * @param text new text to display
     */
    void set_text(const std::string& text);

    /**
     * Set label position
     *
     * @param position 3d position of the label
     */
    void set_position(const vec3d& position);

    /**
     * Set position of the bottom left corner of the label
     *
     * @param x horizontal position [0, 1]
     * @param y vertical position [0, 1]
     * @param align_right interpret position as bottom right corner
     */
    void set_position(float x, float y, bool align_right = false);

    /**
     * Set scaling factor of the label
     *
     * @param scale text scaling factor
     */
    void set_scale(float scale);

    friend class impl::GLLabel;
};

extern const size_t spezia_n;
extern const float spezia_palette[][3];

extern const size_t calref_n;
extern const float calref_palette[][3];

}  // namespace viz
}  // namespace ouster
