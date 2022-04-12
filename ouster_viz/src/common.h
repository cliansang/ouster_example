#pragma once

#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "glfw.h"

namespace ouster {
namespace viz {
namespace impl {

/**
 * load and compile GLSL shaders
 *
 * @param vertex_shader_code code of vertex shader
 * @param fragment_shader_code code of fragment shader
 * @return handle to program_id
 */
inline GLuint load_shaders(const std::string& vertex_shader_code,
                           const std::string& fragment_shader_code) {
    // Adapted from WTFPL-licensed code:
    // https://github.com/opengl-tutorials/ogl/blob/2.1_branch/common/shader.cpp

    // Create the shaders
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    GLint result = GL_FALSE;
    int info_log_length;

    // Compile Vertex Shader
    char const* vertex_source_pointer = vertex_shader_code.c_str();
    glShaderSource(vertex_shader_id, 1, &vertex_source_pointer, NULL);
    glCompileShader(vertex_shader_id);

    // Check Vertex Shader
    glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(vertex_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0) {
        std::vector<char> vertex_shader_error_message(info_log_length + 1);
        glGetShaderInfoLog(vertex_shader_id, info_log_length, NULL,
                           &vertex_shader_error_message[0]);
        printf("%s\n", &vertex_shader_error_message[0]);
    }

    // Compile Fragment Shader
    char const* fragment_source_pointer = fragment_shader_code.c_str();
    glShaderSource(fragment_shader_id, 1, &fragment_source_pointer, NULL);
    glCompileShader(fragment_shader_id);

    // Check Fragment Shader
    glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(fragment_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0) {
        std::vector<char> fragment_shader_error_message(info_log_length + 1);
        glGetShaderInfoLog(fragment_shader_id, info_log_length, NULL,
                           &fragment_shader_error_message[0]);
        printf("%s\n", &fragment_shader_error_message[0]);
    }

    // Link the program
    GLuint program_id = glCreateProgram();
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);
    glLinkProgram(program_id);

    // Check the program
    glGetProgramiv(program_id, GL_LINK_STATUS, &result);
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0) {
        std::vector<char> program_error_message(info_log_length + 1);
        glGetProgramInfoLog(program_id, info_log_length, NULL,
                            &program_error_message[0]);
        printf("%s\n", &program_error_message[0]);
    }

    glDetachShader(program_id, vertex_shader_id);
    glDetachShader(program_id, fragment_shader_id);

    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    return program_id;
}

/**
 * load a texture from an array of GLfloat or equivalent
 * such as float[n][3]
 *
 * @param texture array of at least size width * height * elements_per_texel
 *                where elements per texel is 3 for GL_RGB and 1 for GL_RED
 * @param width   width of texture in texels
 * @param height  height of texture in texels
 * @param texture_id handle generated by glGenTextures
 * @param internal_format internal format, e.g. GL_RGB or GL_RGB32F
 * @param format  format, e.g. GL_RGB or GL_RED
 */
template <class F>
void load_texture(const F& texture, const size_t width, const size_t height,
                  const GLuint texture_id,
                  const GLenum internal_format = GL_RGB,
                  const GLenum format = GL_RGB) {
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // we have only 1 level, so we override base/max levels
    // https://www.khronos.org/opengl/wiki/Common_Mistakes#Creating_a_complete_texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format,
                 GL_FLOAT, texture);
}

/**
 * The point vertex shader supports transforming the point cloud by an array of
 * transformations.
 *
 * @param xyz            XYZ point before it was multiplied by range.
 *                       Corresponds to the "xyzlut" used by LidarScan.
 *
 * @param range          Range of each point.
 *
 * @param key            Key for colouring each point for aesthetic reasons.
 *
 * @param trans_index    Index of which of the transformations to use for this
 *                       point. Normalized between 0 and 1. (0 being the first
 *                       1 being the last).
 *
 * @param model          Extrinsic calibration of the lidar.
 *
 * @param transformation The w transformations are stored as a w x 4 texture.
 *                       Each column of the texture corresponds one 4 x 4
 *                       transformation matrix, where the four pixels' rgb
 *                       values correspond to four columns (3 rotation 1
 *                       translation)
 *
 * @param proj_view      Camera view matrix controlled by the visualizer.
 */
static const std::string point_vertex_shader_code =
    R"SHADER(
            #version 330 core

            in vec3 xyz;
            in vec3 offset;
            in float range;
            in float key;
            in vec4 mask;
            in float trans_index;

            uniform sampler2D transformation;
            uniform mat4 model;
            uniform mat4 proj_view;

            out float vcolor;
            out vec4 overlay_rgba;
            void main(){
                vec4 local_point = range > 0
                                   ? model * vec4(xyz * range + offset, 1.0)
                                   : vec4(0, 0, 0, 1.0);
                // Here, we get the four columns of the transformation.
                // Since this version of GLSL doesn't have texel fetch,
                // we use texture2D instead. Numbers are chosen to index
                // the middle of each pixel.
                // |     r0     |     r1     |     r2     |     t     |
                // 0   0.125  0.25  0.375   0.5  0.625  0.75  0.875   1
                vec4 r0 = texture(transformation, vec2(trans_index, 0.125));
                vec4 r1 = texture(transformation, vec2(trans_index, 0.375));
                vec4 r2 = texture(transformation, vec2(trans_index, 0.625));
                vec4 t = texture(transformation, vec2(trans_index, 0.875));
                mat4 car_pose = mat4(
                    r0.x, r0.y, r0.z, 0,
                    r1.x, r1.y, r1.z, 0,
                    r2.x, r2.y, r2.z, 0,
                     t.x,  t.y,  t.z, 1
                );

                gl_Position = proj_view * car_pose * local_point;
                vcolor = sqrt(key);
                overlay_rgba = mask;
            })SHADER";
static const std::string point_fragment_shader_code =
    R"SHADER(
            #version 330 core
            in float vcolor;
            in vec4 overlay_rgba;
            uniform sampler2D palette;
            out vec4 color;
            void main() {
                color = vec4(texture(palette, vec2(vcolor, 1)).xyz * (1.0 - overlay_rgba.w)
                             + overlay_rgba.xyz * overlay_rgba.w, 1);
            })SHADER";
static const std::string ring_vertex_shader_code =
    R"SHADER(
            #version 330 core
            in vec3 ring_xyz;
            uniform float ring_range;
            uniform mat4 proj_view;
            void main(){
                gl_Position = proj_view * vec4(ring_xyz * ring_range, 1.0);
                gl_Position.z = gl_Position.w;
            })SHADER";
static const std::string ring_fragment_shader_code =
    R"SHADER(
            #version 330 core
            out vec4 color;
            void main() {
                color = vec4(0.15, 0.15, 0.15, 1);
            })SHADER";
static const std::string cuboid_vertex_shader_code =
    R"SHADER(
            #version 330 core
            in vec3 cuboid_xyz;
            uniform vec4 cuboid_rgba;
            uniform mat4 proj_view;
            out vec4 rgba;
            void main(){
                gl_Position = proj_view * vec4(cuboid_xyz, 1.0);
                rgba = cuboid_rgba;
            })SHADER";
static const std::string cuboid_fragment_shader_code =
    R"SHADER(
            #version 330 core
            in vec4 rgba;
            out vec4 color;
            void main() {
                color = rgba;
            })SHADER";
static const std::string image_vertex_shader_code =
    R"SHADER(
            #version 330 core
            in vec2 vertex;
            in vec2 vertex_uv;
            out vec2 uv;
            void main() {
                gl_Position = vec4(vertex, -1, 1);
                uv = vertex_uv;
            })SHADER";
static const std::string image_fragment_shader_code =
    R"SHADER(
            #version 330 core
            in vec2 uv;
            uniform sampler2D image;
            uniform sampler2D mask;
            out vec4 color;
            void main() {
                vec4 m = texture(mask, uv);
                float a = m.a;
                float r = sqrt(texture(image, uv).r) * (1.0 - a);
                color = vec4(vec3(r, r, r) + m.rgb * a, 1.0);
            })SHADER";

}  // namespace impl
}  // namespace viz
}  // namespace ouster
