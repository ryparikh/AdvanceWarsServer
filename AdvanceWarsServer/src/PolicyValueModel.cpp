#include "PolicyValueModel.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>

#include <ATen/CPUGeneratorImpl.h>

#include "ActionSpace.h"
#include "StateTensor.h"
#include "nlohmann/json.hpp"

namespace {
using ordered_json = nlohmann::ordered_json;
using json = nlohmann::json;

constexpr int kMetadataVersion = 1;
constexpr const char* kModelVersion = "standard-gl-policy-value-v1";
constexpr int kPolicyPlaneCount = 2503;
constexpr int kGlobalActionCount = 3;
constexpr int kValueSize = 1;
constexpr double kHeadInitStdDev = 0.01;

void SetError(PolicyValueModelError& error, const std::string& code, const std::string& message) noexcept {
	error.code = code;
	error.message = message;
}

void ClearError(PolicyValueModelError& error) noexcept {
	error.code.clear();
	error.message.clear();
}

std::int64_t ValuesPerState() noexcept {
	return static_cast<std::int64_t>(StateTensor::ChannelCount()) *
		StateTensor::BoardHeight() *
		StateTensor::BoardWidth();
}

bool PolicyLayoutMatchesActionSpace() noexcept {
	return kPolicyPlaneCount * StateTensor::BoardHeight() * StateTensor::BoardWidth() + kGlobalActionCount == ActionSpace::ActionCount();
}

class PolicyValueResidualBlockImpl final : public torch::nn::Module {
public:
	PolicyValueResidualBlockImpl(int channels, int normGroups) :
		m_conv1(torch::nn::Conv2dOptions(channels, channels, 3).padding(1)),
		m_norm1(torch::nn::GroupNormOptions(normGroups, channels)),
		m_conv2(torch::nn::Conv2dOptions(channels, channels, 3).padding(1)),
		m_norm2(torch::nn::GroupNormOptions(normGroups, channels)) {
		register_module("conv1", m_conv1);
		register_module("norm1", m_norm1);
		register_module("conv2", m_conv2);
		register_module("norm2", m_norm2);
	}

	torch::Tensor forward(torch::Tensor input) {
		torch::Tensor residual = input;
		torch::Tensor x = torch::relu(m_norm1->forward(m_conv1->forward(input)));
		x = m_norm2->forward(m_conv2->forward(x));
		return torch::relu(x + residual);
	}

	torch::nn::Conv2d m_conv1{ nullptr };
	torch::nn::GroupNorm m_norm1{ nullptr };
	torch::nn::Conv2d m_conv2{ nullptr };
	torch::nn::GroupNorm m_norm2{ nullptr };
};

TORCH_MODULE(PolicyValueResidualBlock);

void InitializeConv(torch::nn::Conv2d& conv, bool smallHead) {
	if (smallHead) {
		torch::nn::init::normal_(conv->weight, 0.0, kHeadInitStdDev);
	}
	else {
		torch::nn::init::kaiming_normal_(conv->weight, 0.0, torch::kFanOut, torch::kReLU);
	}

	if (conv->bias.defined()) {
		torch::nn::init::zeros_(conv->bias);
	}
}

void InitializeLinear(torch::nn::Linear& linear, bool smallHead) {
	if (smallHead) {
		torch::nn::init::normal_(linear->weight, 0.0, kHeadInitStdDev);
	}
	else {
		torch::nn::init::kaiming_normal_(linear->weight, 0.0, torch::kFanOut, torch::kReLU);
	}

	if (linear->bias.defined()) {
		torch::nn::init::zeros_(linear->bias);
	}
}

void InitializeGroupNorm(torch::nn::GroupNorm& norm) {
	if (norm->weight.defined()) {
		torch::nn::init::ones_(norm->weight);
	}
	if (norm->bias.defined()) {
		torch::nn::init::zeros_(norm->bias);
	}
}

std::string CurrentUtcTimestamp() {
	std::time_t now = std::time(nullptr);
	std::tm utc{};
	gmtime_s(&utc, &now);
	char buffer[32]{};
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
	return buffer;
}

ordered_json BuildMetadata(const PolicyValueModelConfig& config, std::int64_t seed, const std::string& validatedDevice, std::int64_t parameterCount) {
	ordered_json metadata;
	metadata["metadataVersion"] = kMetadataVersion;
	metadata["modelVersion"] = kModelVersion;
	metadata["createdAt"] = CurrentUtcTimestamp();
	metadata["validatedDevice"] = validatedDevice;
	metadata["seed"] = seed;
	metadata["architecture"] = ordered_json{
		{ "hiddenChannels", config.hiddenChannels },
		{ "residualBlocks", config.residualBlocks },
		{ "normGroups", config.normGroups },
		{ "policyPlaneCount", kPolicyPlaneCount },
		{ "globalActionCount", kGlobalActionCount },
	};
	metadata["stateTensor"] = ordered_json{
		{ "version", StateTensor::Version() },
		{ "channels", StateTensor::ChannelCount() },
		{ "height", StateTensor::BoardHeight() },
		{ "width", StateTensor::BoardWidth() },
	};
	metadata["actionSpace"] = ordered_json{
		{ "version", ActionSpace::Version() },
		{ "actionCount", ActionSpace::ActionCount() },
	};
	metadata["outputs"] = ordered_json{
		{ "policyLogits", ActionSpace::ActionCount() },
		{ "value", kValueSize },
	};
	metadata["parameterCount"] = parameterCount;
	return metadata;
}

bool HasOnlyFields(const json& object, const std::vector<std::string>& allowedFields, std::string& invalidField) {
	if (!object.is_object()) {
		invalidField.clear();
		return false;
	}

	for (auto it = object.begin(); it != object.end(); ++it) {
		if (std::find(allowedFields.begin(), allowedFields.end(), it.key()) == allowedFields.end()) {
			invalidField = it.key();
			return false;
		}
	}
	return true;
}

Result RequireObjectFields(const json& object, const std::vector<std::string>& fields, const std::string& label, PolicyValueModelError& error) {
	std::string invalidField;
	if (!HasOnlyFields(object, fields, invalidField)) {
		const std::string message = invalidField.empty() ? label + " must be an object" : label + " contains unknown field: " + invalidField;
		SetError(error, "metadata-schema-error", message);
		return Result::Failed;
	}

	for (const std::string& field : fields) {
		if (!object.contains(field)) {
			SetError(error, "metadata-schema-error", label + " is missing required field: " + field);
			return Result::Failed;
		}
	}
	return Result::Succeeded;
}

template<typename T>
Result RequireValue(const json& object, const std::string& field, const T& expected, const std::string& label, PolicyValueModelError& error) {
	if (!object.contains(field) || object.at(field) != expected) {
		std::ostringstream message;
		message << label << "." << field << " is incompatible";
		SetError(error, "metadata-incompatible", message.str());
		return Result::Failed;
	}
	return Result::Succeeded;
}

Result ParseMetadata(const json& metadata, PolicyValueModelConfig& config, std::int64_t& seed, std::int64_t& parameterCount, PolicyValueModelError& error) {
	IfFailedReturn(RequireObjectFields(metadata, {
		"metadataVersion",
		"modelVersion",
		"createdAt",
		"validatedDevice",
		"seed",
		"architecture",
		"stateTensor",
		"actionSpace",
		"outputs",
		"parameterCount",
	}, "metadata", error));

	IfFailedReturn(RequireValue(metadata, "metadataVersion", kMetadataVersion, "metadata", error));
	IfFailedReturn(RequireValue(metadata, "modelVersion", std::string(kModelVersion), "metadata", error));

	if (!metadata.at("seed").is_number_integer()) {
		SetError(error, "metadata-schema-error", "metadata.seed must be an integer");
		return Result::Failed;
	}
	seed = metadata.at("seed").get<std::int64_t>();
	if (seed < 0) {
		SetError(error, "metadata-schema-error", "metadata.seed must be non-negative");
		return Result::Failed;
	}
	if (!metadata.at("parameterCount").is_number_integer()) {
		SetError(error, "metadata-schema-error", "metadata.parameterCount must be an integer");
		return Result::Failed;
	}
	parameterCount = metadata.at("parameterCount").get<std::int64_t>();
	if (parameterCount <= 0) {
		SetError(error, "metadata-schema-error", "metadata.parameterCount must be positive");
		return Result::Failed;
	}

	const json& architecture = metadata.at("architecture");
	IfFailedReturn(RequireObjectFields(architecture, {
		"hiddenChannels",
		"residualBlocks",
		"normGroups",
		"policyPlaneCount",
		"globalActionCount",
	}, "architecture", error));

	if (!architecture.at("hiddenChannels").is_number_integer() ||
		!architecture.at("residualBlocks").is_number_integer() ||
		!architecture.at("normGroups").is_number_integer()) {
		SetError(error, "metadata-schema-error", "architecture numeric fields must be integers");
		return Result::Failed;
	}

	config.hiddenChannels = architecture.at("hiddenChannels").get<int>();
	config.residualBlocks = architecture.at("residualBlocks").get<int>();
	config.normGroups = architecture.at("normGroups").get<int>();
	IfFailedReturn(RequireValue(architecture, "policyPlaneCount", kPolicyPlaneCount, "architecture", error));
	IfFailedReturn(RequireValue(architecture, "globalActionCount", kGlobalActionCount, "architecture", error));

	const json& stateTensor = metadata.at("stateTensor");
	IfFailedReturn(RequireObjectFields(stateTensor, { "version", "channels", "height", "width" }, "stateTensor", error));
	IfFailedReturn(RequireValue(stateTensor, "version", std::string(StateTensor::Version()), "stateTensor", error));
	IfFailedReturn(RequireValue(stateTensor, "channels", StateTensor::ChannelCount(), "stateTensor", error));
	IfFailedReturn(RequireValue(stateTensor, "height", StateTensor::BoardHeight(), "stateTensor", error));
	IfFailedReturn(RequireValue(stateTensor, "width", StateTensor::BoardWidth(), "stateTensor", error));

	const json& actionSpace = metadata.at("actionSpace");
	IfFailedReturn(RequireObjectFields(actionSpace, { "version", "actionCount" }, "actionSpace", error));
	IfFailedReturn(RequireValue(actionSpace, "version", std::string(ActionSpace::Version()), "actionSpace", error));
	IfFailedReturn(RequireValue(actionSpace, "actionCount", ActionSpace::ActionCount(), "actionSpace", error));

	const json& outputs = metadata.at("outputs");
	IfFailedReturn(RequireObjectFields(outputs, { "policyLogits", "value" }, "outputs", error));
	IfFailedReturn(RequireValue(outputs, "policyLogits", ActionSpace::ActionCount(), "outputs", error));
	IfFailedReturn(RequireValue(outputs, "value", kValueSize, "outputs", error));

	return ValidatePolicyValueModelConfig(config, error);
}

Result PrepareCheckpointDirectory(const std::filesystem::path& checkpointDir, bool force, PolicyValueModelError& error) {
	const std::filesystem::path metadataPath = checkpointDir / "metadata.json";
	const std::filesystem::path modelPath = checkpointDir / "model.pt";

	if (std::filesystem::exists(checkpointDir)) {
		if (!std::filesystem::is_directory(checkpointDir)) {
			SetError(error, "checkpoint-path-not-directory", "checkpoint path exists but is not a directory");
			return Result::Failed;
		}
		if (!force) {
			SetError(error, "checkpoint-exists", "checkpoint directory already exists; pass --force to overwrite known bundle files");
			return Result::Failed;
		}

		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(checkpointDir)) {
			const std::filesystem::path filename = entry.path().filename();
			if (filename != "metadata.json" && filename != "model.pt") {
				SetError(error, "checkpoint-has-unexpected-file", "checkpoint directory contains unexpected file: " + filename.string());
				return Result::Failed;
			}
		}
		std::filesystem::remove(metadataPath);
		std::filesystem::remove(modelPath);
		return Result::Succeeded;
	}

	const std::filesystem::path parent = checkpointDir.parent_path();
	if (!parent.empty()) {
		std::filesystem::create_directories(parent);
	}
	std::filesystem::create_directory(checkpointDir);
	return Result::Succeeded;
}

Result ValidateInputTensor(const torch::Tensor& input, PolicyValueModelError& error) noexcept {
	if (input.dim() != 4) {
		SetError(error, "invalid-input-shape", "input tensor must have shape [N, C, H, W]");
		return Result::Failed;
	}
	if (input.size(0) <= 0) {
		SetError(error, "invalid-input-shape", "input batch size must be positive");
		return Result::Failed;
	}
	if (input.size(1) != StateTensor::ChannelCount() ||
		input.size(2) != StateTensor::BoardHeight() ||
		input.size(3) != StateTensor::BoardWidth()) {
		SetError(error, "invalid-input-shape", "input tensor shape does not match StateTensor NCHW layout");
		return Result::Failed;
	}
	if (input.scalar_type() != torch::kFloat32) {
		SetError(error, "invalid-input-type", "input tensor must be float32");
		return Result::Failed;
	}
	if (!input.is_contiguous()) {
		SetError(error, "invalid-input-layout", "input tensor must be contiguous");
		return Result::Failed;
	}
	return Result::Succeeded;
}
}

PolicyValueNetworkImpl::PolicyValueNetworkImpl(const PolicyValueModelConfig& config) :
	m_config(config),
	m_inputConv(torch::nn::Conv2dOptions(StateTensor::ChannelCount(), config.hiddenChannels, 3).padding(1)),
	m_inputNorm(torch::nn::GroupNormOptions(config.normGroups, config.hiddenChannels)),
	m_residualBlocks(torch::nn::ModuleList()),
	m_policyConv(torch::nn::Conv2dOptions(config.hiddenChannels, kPolicyPlaneCount, 1)),
	m_globalPolicy(config.hiddenChannels, kGlobalActionCount),
	m_valueConv(torch::nn::Conv2dOptions(config.hiddenChannels, 1, 1)),
	m_valueHidden(StateTensor::BoardHeight() * StateTensor::BoardWidth(), config.hiddenChannels),
	m_valueOutput(config.hiddenChannels, kValueSize) {
	register_module("inputConv", m_inputConv);
	register_module("inputNorm", m_inputNorm);
	register_module("residualBlocks", m_residualBlocks);
	for (int i = 0; i < config.residualBlocks; ++i) {
		m_residualBlocks->push_back(PolicyValueResidualBlock(config.hiddenChannels, config.normGroups));
	}
	register_module("policyConv", m_policyConv);
	register_module("globalPolicy", m_globalPolicy);
	register_module("valueConv", m_valueConv);
	register_module("valueHidden", m_valueHidden);
	register_module("valueOutput", m_valueOutput);
}

PolicyValueNetworkOutput PolicyValueNetworkImpl::forward(torch::Tensor input) {
	torch::Tensor x = torch::relu(m_inputNorm->forward(m_inputConv->forward(input)));
	for (std::size_t i = 0; i < m_residualBlocks->size(); ++i) {
		x = m_residualBlocks->at<PolicyValueResidualBlockImpl>(i).forward(x);
	}

	const std::int64_t batchSize = input.size(0);
	torch::Tensor policyPlanes = m_policyConv->forward(x).view({ batchSize, -1 });
	torch::Tensor pooled = torch::adaptive_avg_pool2d(x, { 1, 1 }).view({ batchSize, m_config.hiddenChannels });
	torch::Tensor globalPolicy = m_globalPolicy->forward(pooled);

	torch::Tensor value = torch::relu(m_valueConv->forward(x)).view({ batchSize, -1 });
	value = torch::relu(m_valueHidden->forward(value));
	value = torch::tanh(m_valueOutput->forward(value));

	return {
		torch::cat({ policyPlanes, globalPolicy }, 1).contiguous(),
		value.contiguous(),
	};
}

const PolicyValueModelConfig& PolicyValueNetworkImpl::Config() const noexcept {
	return m_config;
}

void PolicyValueNetworkImpl::ResetParameters() {
	InitializeConv(m_inputConv, false);
	InitializeGroupNorm(m_inputNorm);
	for (std::size_t i = 0; i < m_residualBlocks->size(); ++i) {
		PolicyValueResidualBlockImpl& block = m_residualBlocks->at<PolicyValueResidualBlockImpl>(i);
		InitializeConv(block.m_conv1, false);
		InitializeGroupNorm(block.m_norm1);
		InitializeConv(block.m_conv2, false);
		InitializeGroupNorm(block.m_norm2);
	}

	InitializeConv(m_policyConv, true);
	InitializeLinear(m_globalPolicy, true);
	InitializeConv(m_valueConv, true);
	InitializeLinear(m_valueHidden, false);
	InitializeLinear(m_valueOutput, true);
}

const char* PolicyValueModelVersion() noexcept {
	return kModelVersion;
}

int PolicyValuePolicyPlaneCount() noexcept {
	return kPolicyPlaneCount;
}

int PolicyValueGlobalActionCount() noexcept {
	return kGlobalActionCount;
}

bool PolicyValueCudaAvailable() noexcept {
#ifdef AW_USE_LIBTORCH_CUDA
	return torch::cuda::is_available();
#else
	return false;
#endif
}

int PolicyValueCudaDeviceCount() noexcept {
#ifdef AW_USE_LIBTORCH_CUDA
	return static_cast<int>(torch::cuda::device_count());
#else
	return 0;
#endif
}

Result ValidatePolicyValueModelConfig(const PolicyValueModelConfig& config, PolicyValueModelError& error) noexcept {
	ClearError(error);
	if (config.hiddenChannels <= 0) {
		SetError(error, "invalid-model-config", "hiddenChannels must be positive");
		return Result::Failed;
	}
	if (config.residualBlocks < 0) {
		SetError(error, "invalid-model-config", "residualBlocks must be non-negative");
		return Result::Failed;
	}
	if (config.normGroups <= 0) {
		SetError(error, "invalid-model-config", "normGroups must be positive");
		return Result::Failed;
	}
	if (config.hiddenChannels % config.normGroups != 0) {
		SetError(error, "invalid-model-config", "hiddenChannels must be divisible by normGroups");
		return Result::Failed;
	}
	if (!PolicyLayoutMatchesActionSpace()) {
		SetError(error, "policy-layout-mismatch", "policy planes and global logits do not reconstruct ActionSpace::ActionCount");
		return Result::Failed;
	}
	return Result::Succeeded;
}

torch::Device ResolvePolicyValueDevice(const std::string& deviceName, PolicyValueModelError& error) noexcept {
	ClearError(error);
	if (deviceName == "cpu") {
		return torch::Device(torch::kCPU);
	}
	if (deviceName == "auto") {
		return PolicyValueCudaAvailable() ? torch::Device(torch::kCUDA, 0) : torch::Device(torch::kCPU);
	}
	if (deviceName == "cuda") {
		if (!PolicyValueCudaAvailable()) {
			SetError(error, "cuda-unavailable", "CUDA was requested but is not available");
			return torch::Device(torch::kCPU);
		}
		return torch::Device(torch::kCUDA, 0);
	}

	SetError(error, "invalid-device", "device must be cpu, auto, or cuda");
	return torch::Device(torch::kCPU);
}

std::string PolicyValueDeviceName(const torch::Device& device) {
	if (device.is_cuda()) {
		return "cuda:" + std::to_string(device.index());
	}
	return "cpu";
}

PolicyValueNetwork CreatePolicyValueNetwork(const PolicyValueModelConfig& config, std::int64_t seed) {
	auto cpuGenerator = at::detail::getDefaultCPUGenerator();
	cpuGenerator.set_current_seed(static_cast<std::uint64_t>(seed));
	PolicyValueNetwork model(config);
	model->ResetParameters();
	model->eval();
	return model;
}

std::int64_t CountPolicyValueParameters(const PolicyValueNetwork& model) {
	std::int64_t count = 0;
	for (const torch::Tensor& parameter : model->parameters()) {
		count += parameter.numel();
	}
	return count;
}

Result RunPolicyValueInference(PolicyValueNetwork& model, torch::Tensor input, PolicyValueNetworkOutput& output, PolicyValueModelError& error) noexcept {
	try {
		ClearError(error);
		IfFailedReturn(ValidateInputTensor(input, error));
		torch::NoGradGuard noGrad;
		model->eval();
		output = model->forward(input);
		if (output.policyLogits.size(0) != input.size(0) ||
			output.policyLogits.size(1) != ActionSpace::ActionCount() ||
			output.value.size(0) != input.size(0) ||
			output.value.size(1) != kValueSize) {
			SetError(error, "invalid-output-shape", "model output shape does not match policy/value contract");
			return Result::Failed;
		}
		return Result::Succeeded;
	}
	catch (const std::exception& err) {
		SetError(error, "inference-failed", err.what());
		return Result::Failed;
	}
}

Result RunPolicyValueInference(PolicyValueNetwork& model, const std::vector<float>& flatBatch, int batchSize, PolicyValueNetworkOutput& output, PolicyValueModelError& error) noexcept {
	try {
		ClearError(error);
		if (batchSize <= 0) {
			SetError(error, "invalid-input-shape", "batchSize must be positive");
			return Result::Failed;
		}
		const std::int64_t expectedValues = ValuesPerState() * batchSize;
		if (flatBatch.size() != static_cast<std::size_t>(expectedValues)) {
			SetError(error, "invalid-input-shape", "flat batch size does not match StateTensor shape and batchSize");
			return Result::Failed;
		}

		torch::Tensor input = torch::from_blob(
			const_cast<float*>(flatBatch.data()),
			{ batchSize, StateTensor::ChannelCount(), StateTensor::BoardHeight(), StateTensor::BoardWidth() },
			torch::TensorOptions().dtype(torch::kFloat32)).clone().contiguous();
		return RunPolicyValueInference(model, input, output, error);
	}
	catch (const std::exception& err) {
		SetError(error, "inference-failed", err.what());
		return Result::Failed;
	}
}

Result SavePolicyValueCheckpoint(
	const std::filesystem::path& checkpointDir,
	PolicyValueNetwork& model,
	const PolicyValueModelConfig& config,
	std::int64_t seed,
	const std::string& validatedDevice,
	bool force,
	PolicyValueModelError& error) noexcept {
	try {
		ClearError(error);
		if (seed < 0) {
			SetError(error, "invalid-seed", "seed must be non-negative");
			return Result::Failed;
		}
		IfFailedReturn(ValidatePolicyValueModelConfig(config, error));
		IfFailedReturn(PrepareCheckpointDirectory(checkpointDir, force, error));

		model->to(torch::kCPU);
		model->eval();
		const ordered_json metadata = BuildMetadata(config, seed, validatedDevice, CountPolicyValueParameters(model));

		std::ofstream metadataOutput(checkpointDir / "metadata.json", std::ios::trunc);
		if (!metadataOutput.is_open()) {
			SetError(error, "metadata-write-failed", "metadata.json could not be opened for writing");
			return Result::Failed;
		}
		metadataOutput << metadata.dump(2) << "\n";
		metadataOutput.close();

		torch::save(model, (checkpointDir / "model.pt").string());
		return Result::Succeeded;
	}
	catch (const std::exception& err) {
		SetError(error, "checkpoint-save-failed", err.what());
		return Result::Failed;
	}
}

Result LoadPolicyValueCheckpoint(
	const std::filesystem::path& checkpointDir,
	PolicyValueNetwork& model,
	PolicyValueModelConfig& config,
	std::int64_t& seed,
	PolicyValueModelError& error) noexcept {
	try {
		ClearError(error);
		const std::filesystem::path metadataPath = checkpointDir / "metadata.json";
		const std::filesystem::path modelPath = checkpointDir / "model.pt";
		if (!std::filesystem::exists(metadataPath)) {
			SetError(error, "metadata-missing", "metadata.json is missing from checkpoint bundle");
			return Result::Failed;
		}
		if (!std::filesystem::exists(modelPath)) {
			SetError(error, "model-missing", "model.pt is missing from checkpoint bundle");
			return Result::Failed;
		}

		std::ifstream metadataInput(metadataPath);
		if (!metadataInput.is_open()) {
			SetError(error, "metadata-read-failed", "metadata.json could not be opened");
			return Result::Failed;
		}

		json metadata;
		metadataInput >> metadata;
		std::int64_t metadataParameterCount = 0;
		IfFailedReturn(ParseMetadata(metadata, config, seed, metadataParameterCount, error));

		model = CreatePolicyValueNetwork(config, seed);
		if (CountPolicyValueParameters(model) != metadataParameterCount) {
			SetError(error, "metadata-incompatible", "metadata.parameterCount is incompatible");
			return Result::Failed;
		}
		torch::load(model, modelPath.string());
		model->to(torch::kCPU);
		model->eval();
		return Result::Succeeded;
	}
	catch (const std::exception& err) {
		SetError(error, "checkpoint-load-failed", err.what());
		return Result::Failed;
	}
}
