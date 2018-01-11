#include "Buffers.hpp"

vk::Buffer *Buffers::createBuffer(const std::shared_ptr<Context> context, vk::DeviceSize size, vk::BufferUsageFlags usage)
{
	auto bufferCreateInfo = vk::BufferCreateInfo().setSize(size).setUsage(usage);
	auto buffer = context->getDevice()->createBuffer(bufferCreateInfo);
	return new vk::Buffer(buffer);
}

vk::DeviceMemory *Buffers::createBufferMemory(const std::shared_ptr<Context> context, const vk::Buffer *buffer, vk::DeviceSize size, vk::MemoryPropertyFlags memoryPropertyFlags)
{
	auto memoryRequirements = context->getDevice()->getBufferMemoryRequirements(*buffer);
	auto memoryProperties = context->getPhysicalDevice()->getMemoryProperties();

	uint32_t memoryTypeIndex = 0;
	bool foundMatch = false;
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
	{
		if ((memoryRequirements.memoryTypeBits & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)
		{
			memoryTypeIndex = i;
			foundMatch = true;
			break;
		}
	}

	if (!foundMatch)
	{
		throw std::runtime_error("Failed to find suitable memory type for buffer.");
	}

	auto memoryAllocateInfo = vk::MemoryAllocateInfo().setAllocationSize(memoryRequirements.size).setMemoryTypeIndex(memoryTypeIndex);
	auto deviceMemory = context->getDevice()->allocateMemory(memoryAllocateInfo);
	context->getDevice()->bindBufferMemory(*buffer, deviceMemory, 0);
	return new vk::DeviceMemory(deviceMemory);
}

Buffers::Buffers(const std::shared_ptr<Context> context)
{
	this->context = context;
}

void Buffers::finalize(uint32_t numModels, uint32_t numLights, uint32_t numShadowMaps)
{
	vk::DeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
	vk::DeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

#ifndef MK_OPTIMIZATION_BUFFER_STAGING
	vertexBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, vertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer), bufferDeleter);
	vertexBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, vertexBuffer.get(), vertexBufferSize, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent), bufferMemoryDeleter);
		
	auto memory = context->getDevice()->mapMemory(*vertexBufferMemory, 0, vertexBufferSize);
	memcpy(memory, vertices.data(), vertexBufferSize);
	context->getDevice()->unmapMemory(*vertexBufferMemory);

	indexBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, indexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer), bufferDeleter);
	indexBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, indexBuffer.get(), indexBufferSize, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent), bufferMemoryDeleter);

	memory = context->getDevice()->mapMemory(*indexBufferMemory, 0, indexBufferSize);
	memcpy(memory, indices.data(), indexBufferSize);
	context->getDevice()->unmapMemory(*indexBufferMemory);
#else

	// begin command buffer

	auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo().setCommandPool(*context->getCommandPool()).setCommandBufferCount(1);
	auto commandBuffer = context->getDevice()->allocateCommandBuffers(commandBufferAllocateInfo).at(0);
	auto commandBufferBeginInfo = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	commandBuffer.begin(commandBufferBeginInfo);


	// vertex buffer staging

	std::unique_ptr<vk::Buffer, decltype(bufferDeleter)> stagingVertexBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc), bufferDeleter);
	std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)> stagingVertexBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, stagingVertexBuffer.get(), vertexBufferSize, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent), bufferMemoryDeleter);

	auto memory = context->getDevice()->mapMemory(*stagingVertexBufferMemory, 0, vertexBufferSize);
	memcpy(memory, vertices.data(), vertexBufferSize);
	context->getDevice()->unmapMemory(*stagingVertexBufferMemory);

	vertexBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, vertexBufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer), bufferDeleter);
	vertexBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, vertexBuffer.get(), vertexBufferSize, vk::MemoryPropertyFlagBits::eDeviceLocal), bufferMemoryDeleter);
	commandBuffer.copyBuffer(*stagingVertexBuffer, *vertexBuffer, vk::BufferCopy(0, 0, vertexBufferSize));
	

	// index buffer staging

	std::unique_ptr<vk::Buffer, decltype(bufferDeleter)> stagingIndexBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc), bufferDeleter);
	std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)> stagingIndexBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, stagingIndexBuffer.get(), indexBufferSize, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent), bufferMemoryDeleter);

	memory = context->getDevice()->mapMemory(*stagingIndexBufferMemory, 0, indexBufferSize);
	memcpy(memory, indices.data(), indexBufferSize);
	context->getDevice()->unmapMemory(*stagingIndexBufferMemory);

	indexBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, indexBufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer), bufferDeleter);
	indexBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, indexBuffer.get(), indexBufferSize, vk::MemoryPropertyFlagBits::eDeviceLocal), bufferMemoryDeleter);
	commandBuffer.copyBuffer(*stagingIndexBuffer, *indexBuffer, vk::BufferCopy(0, 0, indexBufferSize));
	
	
	// end command buffer

	commandBuffer.end();
	auto submitInfo = vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(&commandBuffer);
	context->getQueue().submit({ submitInfo }, nullptr);
	context->getQueue().waitIdle();
	context->getDevice()->freeCommandBuffers(*context->getCommandPool(), 1, &commandBuffer);

#endif


#ifndef MK_OPTIMIZATION_PUSH_CONSTANTS
	
	auto minUboAlignment = context->getPhysicalDevice()->getProperties().limits.minUniformBufferOffsetAlignment;
	dataAlignment = sizeof(glm::mat4);
	if (minUboAlignment > 0)
	{
		dataAlignment = (dataAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}

#ifdef MK_OPTIMIZATION_GLOBAL_UNIFORM_BUFFERS

	vk::DeviceSize bufferSize = sizeof(UniformBufferData);
	uniformBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer), bufferDeleter);
	uniformBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, uniformBuffer.get(), bufferSize, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent), bufferMemoryDeleter);

	

	bufferSize = numShadowMaps * dataAlignment + numModels * dataAlignment + 2 * numLights * dataAlignment;
	dynamicUniformBufferData.lightViewProjectionMatrix = (glm::mat4*)_aligned_malloc(bufferSize, /*numShadowMaps */ dataAlignment);
	dynamicUniformBufferData.worldMatrix = (glm::mat4*)_aligned_malloc(bufferSize, /*numModels */ dataAlignment);
	dynamicUniformBufferData.worldViewProjectionMatrix = (glm::mat4*)_aligned_malloc(bufferSize, /*numLights */ dataAlignment);
	dynamicUniformBufferData.lightData = (glm::mat4*)_aligned_malloc(bufferSize, /*numLights */ dataAlignment);

	dynamicUniformBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer), bufferDeleter);
	dynamicUniformBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, dynamicUniformBuffer.get(), bufferSize, vk::MemoryPropertyFlagBits::eHostVisible), bufferMemoryDeleter);

#else

	// shadow pass vertex dynamic uniform buffer
	vk::DeviceSize bufferSize = numShadowMaps * dataAlignment;
	shadowPassDynamicData.lightViewProjectionMatrix = (glm::mat4*)_aligned_malloc(bufferSize, dataAlignment);
	shadowPassDynamicUniformBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer), bufferDeleter);
	shadowPassDynamicUniformBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, shadowPassDynamicUniformBuffer.get(), bufferSize, vk::MemoryPropertyFlagBits::eHostVisible), bufferMemoryDeleter);

	// geometry pass vertex dynamic uniform buffer
	bufferSize = numModels * dataAlignment;
	geometryPassVertexDynamicData.worldMatrix = (glm::mat4*)_aligned_malloc(bufferSize, dataAlignment);
	geometryPassVertexDynamicUniformBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer), bufferDeleter);
	geometryPassVertexDynamicUniformBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, geometryPassVertexDynamicUniformBuffer.get(), bufferSize, vk::MemoryPropertyFlagBits::eHostVisible), bufferMemoryDeleter);

	// geometry pass vertex uniform buffer
	bufferSize = sizeof(GeometryPassVertexData);
	geometryPassVertexUniformBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer), bufferDeleter);
	geometryPassVertexUniformBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, geometryPassVertexUniformBuffer.get(), bufferSize, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent), bufferMemoryDeleter);

	// lighting pass vertex dynamic uniform buffer
	bufferSize = numLights * dataAlignment;
	lightingPassVertexDynamicData.worldViewProjectionMatrix = (glm::mat4*)_aligned_malloc(bufferSize, dataAlignment);
	lightingPassVertexDynamicUniformBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer), bufferDeleter);
	lightingPassVertexDynamicUniformBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, lightingPassVertexDynamicUniformBuffer.get(), bufferSize, vk::MemoryPropertyFlagBits::eHostVisible), bufferMemoryDeleter);

	// lighting pass fragment dynamic uniform buffer
	bufferSize = numLights * dataAlignment;
	lightingPassFragmentDynamicData.lightData = (glm::mat4*)_aligned_malloc(bufferSize / 2, dataAlignment);
	lightingPassFragmentDynamicUniformBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer), bufferDeleter);
	lightingPassFragmentDynamicUniformBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, lightingPassFragmentDynamicUniformBuffer.get(), bufferSize, vk::MemoryPropertyFlagBits::eHostVisible), bufferMemoryDeleter);

	// lighting pass fragment uniform buffer
	bufferSize = sizeof(LightingPassFragmentData);
	lightingPassFragmentUniformBuffer = std::unique_ptr<vk::Buffer, decltype(bufferDeleter)>(createBuffer(context, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer), bufferDeleter);
	lightingPassFragmentUniformBufferMemory = std::unique_ptr<vk::DeviceMemory, decltype(bufferMemoryDeleter)>(createBufferMemory(context, lightingPassFragmentUniformBuffer.get(), bufferSize, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent), bufferMemoryDeleter);

#endif

#endif
}