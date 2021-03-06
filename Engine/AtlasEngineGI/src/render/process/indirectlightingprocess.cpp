#include "indirectlightingprocess.h"
#include "scene.h"
#include "quad.h"
#include "neuralnetworkloader.h"
#include "neuralnetwork.h"
#include "qapplication.h"

/*
* Utility functions
* */
static int nextPowerOfTwo(int x) {
	x--;
	x |= x >> 1; // handle 2 bit numbers
	x |= x >> 2; // handle 4 bit numbers
	x |= x >> 4; // handle 8 bit numbers
	x |= x >> 8; // handle 16 bit numbers
	x |= x >> 16; // handle 32 bit numbers
	x++;
	return x;
}

IndirectLightingProcess::IndirectLightingProcess() :
	RenderProcess(2)
{
	
}

void IndirectLightingProcess::init(const GLuint &width, const GLuint &height)
{
	RenderProcess::init(width, height);

	m_shader = ComputeShader("shaders/neural_network.comp");
	
	/*
	* Inputs
	* */
	m_shader.use();
	glUniform1i(glGetUniformLocation(m_shader.getProgram(), "gPositionDepth"), 0);
	glUniform1i(glGetUniformLocation(m_shader.getProgram(), "gNormal"), 1);
	glUseProgram(0);

	/*
	* Creating the Output Texture
	* */
	m_out_texture = Texture(GL_RGBA32F, width, height, GL_RGBA, GL_FLOAT,
		NULL, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR);

	m_tex_w = width;
	m_tex_h = height;	 

	// Neural network buffer
	GLuint neural_network_buffer;
	glGenBuffers(1, &neural_network_buffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, neural_network_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(NeuralNetwork), NULL, GL_STATIC_DRAW);
	GLint bufMask = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT ; // invalidate makes a ig difference when re-writting

	NeuralNetwork *neural_network;
	neural_network = (NeuralNetwork *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(NeuralNetwork), bufMask);

	NeuralNetworkLoader neural_network_loader;
	neural_network_loader.loadFile("networksXML/neural_network_pierre.xml", *neural_network);
	
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, neural_network_buffer);

	// Outputs
	m_out_textures.push_back(&m_out_texture);

}

void IndirectLightingProcess::initMenuElement()
{

}

void IndirectLightingProcess::resize(const GLuint &width, const GLuint &height)
{
	RenderProcess::resize(width, height);

	m_out_texture = Texture(GL_RGBA32F, width, height, GL_RGBA, GL_FLOAT,
		NULL, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR);

	m_tex_w = width;
	m_tex_h = height;
}

void IndirectLightingProcess::process(const Quad &quad, const Scene &scene, const GLfloat &render_time, const GLboolean(&keys)[1024])
{
	// Working groups
	const int localWorkGroupSize = 32;

	// Inputs
	glm::vec3 camera_position = scene.getCurrentCamera()->getPosition();
	glm::vec3 point_light_position = scene.getPointLight(0)->getPosition();

	m_out_texture.bindImage(0);

	// Launch compute shader
	m_shader.use();
	
	glActiveTexture(GL_TEXTURE0);
	bindPreviousTexture(0);
	glActiveTexture(GL_TEXTURE1);
	bindPreviousTexture(1);

	// Uniform inputs
	glUniform3f(glGetUniformLocation(m_shader.getProgram(), "camera_position"), camera_position.x, camera_position.y, camera_position.z);
	glUniform3f(glGetUniformLocation(m_shader.getProgram(), "point_light_position"), point_light_position.x, point_light_position.y, point_light_position.z);

	glDispatchCompute(nextPowerOfTwo(m_tex_w / localWorkGroupSize), nextPowerOfTwo(m_tex_h / localWorkGroupSize), 1); // using working spaces
	//glDispatchCompute(m_tex_w, m_tex_h, 1); //using no working spaces

	// Prevent samplign before all writes to image are done
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

}