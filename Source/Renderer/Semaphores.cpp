#include "Semaphores.hpp"

vk::Semaphore *Semaphores::createSemaphore(const std::shared_ptr<Context> context)
{
	auto semaphoreCreateInfo = vk::SemaphoreCreateInfo();
	auto semaphore = context->getDevice()->createSemaphore(semaphoreCreateInfo);
	return new vk::Semaphore(semaphore);
}

Semaphores::Semaphores(const std::shared_ptr<Context> context)
{
	this->context = context;

	imageAvailableSemaphore = std::unique_ptr<vk::Semaphore, decltype(semaphoreDeleter)>(createSemaphore(context), semaphoreDeleter);
	shadowPassDoneSemaphore = std::unique_ptr<vk::Semaphore, decltype(semaphoreDeleter)>(createSemaphore(context), semaphoreDeleter);
	geometryPassDoneSemaphore = std::unique_ptr<vk::Semaphore, decltype(semaphoreDeleter)>(createSemaphore(context), semaphoreDeleter);
	lightingPassDoneSemaphore = std::unique_ptr<vk::Semaphore, decltype(semaphoreDeleter)>(createSemaphore(context), semaphoreDeleter);
	compositePassDoneSemaphore = std::unique_ptr<vk::Semaphore, decltype(semaphoreDeleter)>(createSemaphore(context), semaphoreDeleter);
}