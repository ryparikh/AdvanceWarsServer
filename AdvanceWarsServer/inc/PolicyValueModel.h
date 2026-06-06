#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <torch/torch.h>

#include "Result.h"

struct PolicyValueModelConfig {
	int hiddenChannels{ 64 };
	int residualBlocks{ 4 };
	int normGroups{ 8 };
};

struct PolicyValueModelError {
	std::string code;
	std::string message;
};

struct PolicyValueNetworkOutput {
	torch::Tensor policyLogits;
	torch::Tensor value;
};

class PolicyValueNetworkImpl : public torch::nn::Module {
public:
	explicit PolicyValueNetworkImpl(const PolicyValueModelConfig& config);
	PolicyValueNetworkOutput forward(torch::Tensor input);
	const PolicyValueModelConfig& Config() const noexcept;
	void ResetParameters();

private:
	PolicyValueModelConfig m_config;
	torch::nn::Conv2d m_inputConv{ nullptr };
	torch::nn::GroupNorm m_inputNorm{ nullptr };
	torch::nn::ModuleList m_residualBlocks{ nullptr };
	torch::nn::Conv2d m_policyConv{ nullptr };
	torch::nn::Linear m_globalPolicy{ nullptr };
	torch::nn::Conv2d m_valueConv{ nullptr };
	torch::nn::Linear m_valueHidden{ nullptr };
	torch::nn::Linear m_valueOutput{ nullptr };
};

TORCH_MODULE(PolicyValueNetwork);

const char* PolicyValueModelVersion() noexcept;
int PolicyValuePolicyPlaneCount() noexcept;
int PolicyValueGlobalActionCount() noexcept;
bool PolicyValueCudaAvailable() noexcept;
int PolicyValueCudaDeviceCount() noexcept;
Result ValidatePolicyValueModelConfig(const PolicyValueModelConfig& config, PolicyValueModelError& error) noexcept;
torch::Device ResolvePolicyValueDevice(const std::string& deviceName, PolicyValueModelError& error) noexcept;
std::string PolicyValueDeviceName(const torch::Device& device);
PolicyValueNetwork CreatePolicyValueNetwork(const PolicyValueModelConfig& config, std::int64_t seed);
std::int64_t CountPolicyValueParameters(const PolicyValueNetwork& model);
Result RunPolicyValueInference(PolicyValueNetwork& model, torch::Tensor input, PolicyValueNetworkOutput& output, PolicyValueModelError& error) noexcept;
Result RunPolicyValueInference(PolicyValueNetwork& model, const std::vector<float>& flatBatch, int batchSize, PolicyValueNetworkOutput& output, PolicyValueModelError& error) noexcept;
Result SavePolicyValueCheckpoint(
	const std::filesystem::path& checkpointDir,
	PolicyValueNetwork& model,
	const PolicyValueModelConfig& config,
	std::int64_t seed,
	const std::string& validatedDevice,
	bool force,
	PolicyValueModelError& error) noexcept;
Result LoadPolicyValueCheckpoint(
	const std::filesystem::path& checkpointDir,
	PolicyValueNetwork& model,
	PolicyValueModelConfig& config,
	std::int64_t& seed,
	PolicyValueModelError& error) noexcept;
