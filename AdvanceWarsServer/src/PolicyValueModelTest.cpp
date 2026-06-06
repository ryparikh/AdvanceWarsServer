#include "PolicyValueModelTest.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "ActionSpace.h"
#include "PolicyValueModel.h"
#include "StandardGameSetup.h"
#include "StateTensor.h"

namespace {
bool Expect(bool condition, const std::string& message) {
	if (!condition) {
		std::cerr << "Policy/value model test failed: " << message << std::endl;
		return false;
	}

	return true;
}

bool TensorClose(const torch::Tensor& left, const torch::Tensor& right, double tolerance = 1e-6) {
	if (!left.sizes().equals(right.sizes())) {
		return false;
	}

	return torch::allclose(left.cpu(), right.cpu(), tolerance, tolerance);
}

std::filesystem::path MakeTempCheckpointPath(const std::string& name) {
	return std::filesystem::temp_directory_path() / ("advance-wars-" + name);
}

torch::Tensor MakeSyntheticInput(int batchSize) {
	const std::int64_t valuesPerState = static_cast<std::int64_t>(StateTensor::ChannelCount() * StateTensor::BoardHeight() * StateTensor::BoardWidth());
	return torch::linspace(0.0f, 1.0f, batchSize * valuesPerState, torch::kFloat32)
		.view({ batchSize, StateTensor::ChannelCount(), StateTensor::BoardHeight(), StateTensor::BoardWidth() })
		.contiguous();
}

bool RewriteMetadata(const std::filesystem::path& checkpointPath, const std::function<void(json&)>& mutate) {
	std::ifstream input(checkpointPath / "metadata.json");
	if (!Expect(input.is_open(), "metadata.json should open for mutation")) {
		return false;
	}

	json metadata;
	input >> metadata;
	input.close();
	mutate(metadata);

	std::ofstream output(checkpointPath / "metadata.json", std::ios::trunc);
	if (!Expect(output.is_open(), "metadata.json should reopen for mutation")) {
		return false;
	}
	output << metadata.dump(2) << "\n";
	return true;
}

bool LoadCheckpointShouldFail(const std::filesystem::path& checkpointPath, const std::string& expectedCode) {
	PolicyValueNetwork loaded(nullptr);
	PolicyValueModelConfig loadedConfig;
	std::int64_t loadedSeed = -1;
	PolicyValueModelError error;
	const bool failed = Expect(LoadPolicyValueCheckpoint(checkpointPath, loaded, loadedConfig, loadedSeed, error) == Result::Failed, "checkpoint load should fail");
	return failed && Expect(error.code == expectedCode, "checkpoint load should fail with " + expectedCode + ", got " + error.code);
}

bool ForwardOutputHasExpectedContract(const PolicyValueModelConfig& config, const std::string& deviceName) {
	PolicyValueModelError error;
	const torch::Device device = ResolvePolicyValueDevice(deviceName, error);
	if (!Expect(error.code.empty(), "device should resolve for model test: " + error.message)) {
		return false;
	}

	PolicyValueNetwork model = CreatePolicyValueNetwork(config, 0);
	model->to(device);

	PolicyValueNetworkOutput output;
	if (!Expect(RunPolicyValueInference(model, MakeSyntheticInput(2).to(device), output, error) == Result::Succeeded, "synthetic inference should succeed: " + error.message)) {
		return false;
	}

	return Expect(output.policyLogits.sizes() == torch::IntArrayRef({ 2, ActionSpace::ActionCount() }), "policy logits should align with action space") &&
		Expect(output.value.sizes() == torch::IntArrayRef({ 2, 1 }), "value output should be one scalar per batch item") &&
		Expect(output.policyLogits.is_contiguous(), "policy logits should be contiguous") &&
		Expect(output.value.is_contiguous(), "value output should be contiguous") &&
		Expect(torch::isfinite(output.policyLogits).all().item<bool>(), "policy logits should be finite") &&
		Expect(torch::isfinite(output.value).all().item<bool>(), "value output should be finite") &&
		Expect(output.value.min().item<float>() >= -1.0f && output.value.max().item<float>() <= 1.0f, "value output should be in [-1, 1]");
}

bool RealStandardStateRunsThroughVectorWrapper() {
	json request;
	StandardGameSetupError setupError;
	if (!Expect(BuildStandardGameRequestFromSlots("mcts", "andy", "adder", StandardSettingsJson(), 101, request, setupError) == Result::Succeeded, "standard setup request should build")) {
		return false;
	}

	StandardGameSetupResult setup;
	if (!Expect(CreateStandardGameFromRequest(request, setup, setupError) == Result::Succeeded, "standard setup should create a game")) {
		return false;
	}

	std::vector<float> values;
	if (!Expect(StateTensor::Encode(setup.gameState, values) == Result::Succeeded, "state tensor should encode a standard game")) {
		return false;
	}

	PolicyValueModelConfig config;
	PolicyValueNetwork model = CreatePolicyValueNetwork(config, 0);
	PolicyValueNetworkOutput output;
	PolicyValueModelError error;
	if (!Expect(RunPolicyValueInference(model, values, 1, output, error) == Result::Succeeded, "vector wrapper inference should succeed: " + error.message)) {
		return false;
	}

	return Expect(output.policyLogits.sizes() == torch::IntArrayRef({ 1, ActionSpace::ActionCount() }), "real-state policy logits should align with action space") &&
		Expect(output.value.sizes() == torch::IntArrayRef({ 1, 1 }), "real-state value should be one scalar");
}

bool PolicyLayoutInvariantIsEnforced() {
	PolicyValueModelConfig config;
	PolicyValueModelError error;
	if (!Expect(ValidatePolicyValueModelConfig(config, error) == Result::Succeeded, "default config should be valid: " + error.message)) {
		return false;
	}

	config.normGroups = 7;
	if (!Expect(ValidatePolicyValueModelConfig(config, error) == Result::Failed, "norm groups that do not divide channels should be rejected")) {
		return false;
	}

	return Expect(PolicyValuePolicyPlaneCount() * StateTensor::BoardHeight() * StateTensor::BoardWidth() + PolicyValueGlobalActionCount() == ActionSpace::ActionCount(),
		"policy plane/global split should reconstruct the action count");
}

bool SeededInitializationIsDeterministic() {
	PolicyValueModelConfig config;
	const torch::Tensor input = MakeSyntheticInput(1);
	PolicyValueModelError error;

	PolicyValueNetwork first = CreatePolicyValueNetwork(config, 123);
	PolicyValueNetwork second = CreatePolicyValueNetwork(config, 123);
	PolicyValueNetwork different = CreatePolicyValueNetwork(config, 124);

	PolicyValueNetworkOutput firstOutput;
	PolicyValueNetworkOutput secondOutput;
	PolicyValueNetworkOutput differentOutput;
	if (!Expect(RunPolicyValueInference(first, input, firstOutput, error) == Result::Succeeded, "first seeded inference should succeed")) {
		return false;
	}
	if (!Expect(RunPolicyValueInference(second, input, secondOutput, error) == Result::Succeeded, "second seeded inference should succeed")) {
		return false;
	}
	if (!Expect(RunPolicyValueInference(different, input, differentOutput, error) == Result::Succeeded, "different seeded inference should succeed")) {
		return false;
	}

	return Expect(TensorClose(firstOutput.policyLogits, secondOutput.policyLogits), "same seed should produce identical policy logits") &&
		Expect(TensorClose(firstOutput.value, secondOutput.value), "same seed should produce identical values") &&
		Expect(!TensorClose(firstOutput.policyLogits, differentOutput.policyLogits) || !TensorClose(firstOutput.value, differentOutput.value), "different seed should change at least one output");
}

bool CheckpointRoundTripPreservesOutputs() {
	const std::filesystem::path checkpointPath = MakeTempCheckpointPath("policy-value-roundtrip");
	std::filesystem::remove_all(checkpointPath);

	PolicyValueModelConfig config;
	const std::int64_t seed = 77;
	PolicyValueModelError error;
	PolicyValueNetwork model = CreatePolicyValueNetwork(config, seed);
	PolicyValueNetworkOutput before;
	const torch::Tensor input = MakeSyntheticInput(1);
	if (!Expect(RunPolicyValueInference(model, input, before, error) == Result::Succeeded, "pre-save inference should succeed: " + error.message)) {
		return false;
	}

	if (!Expect(SavePolicyValueCheckpoint(checkpointPath, model, config, seed, "cpu", false, error) == Result::Succeeded, "checkpoint save should succeed: " + error.message)) {
		return false;
	}

	PolicyValueNetwork loaded(nullptr);
	PolicyValueModelConfig loadedConfig;
	std::int64_t loadedSeed = -1;
	if (!Expect(LoadPolicyValueCheckpoint(checkpointPath, loaded, loadedConfig, loadedSeed, error) == Result::Succeeded, "checkpoint load should succeed: " + error.message)) {
		std::filesystem::remove_all(checkpointPath);
		return false;
	}

	PolicyValueNetworkOutput after;
	const bool inferenceSucceeded = Expect(RunPolicyValueInference(loaded, input, after, error) == Result::Succeeded, "post-load inference should succeed: " + error.message);
	const bool outputsMatch = inferenceSucceeded &&
		Expect(TensorClose(before.policyLogits, after.policyLogits), "checkpoint policy logits should round trip") &&
		Expect(TensorClose(before.value, after.value), "checkpoint value should round trip");
	std::filesystem::remove_all(checkpointPath);
	return outputsMatch;
}

bool InvalidCheckpointMetadataIsRejected() {
	const std::filesystem::path checkpointPath = MakeTempCheckpointPath("policy-value-invalid");
	std::filesystem::remove_all(checkpointPath);

	PolicyValueModelConfig config;
	PolicyValueNetwork model = CreatePolicyValueNetwork(config, 0);
	PolicyValueModelError error;
	if (!Expect(SavePolicyValueCheckpoint(checkpointPath, model, config, 0, "cpu", false, error) == Result::Succeeded, "baseline checkpoint save should succeed")) {
		return false;
	}

	std::filesystem::remove(checkpointPath / "model.pt");
	bool passed = LoadCheckpointShouldFail(checkpointPath, "model-missing");

	if (!Expect(SavePolicyValueCheckpoint(checkpointPath, model, config, 0, "cpu", true, error) == Result::Succeeded, "baseline checkpoint should resave with force")) {
		std::filesystem::remove_all(checkpointPath);
		return false;
	}
	passed = RewriteMetadata(checkpointPath, [](json& metadata) {
		metadata["unknownField"] = true;
	}) && LoadCheckpointShouldFail(checkpointPath, "metadata-schema-error") && passed;

	if (!Expect(SavePolicyValueCheckpoint(checkpointPath, model, config, 0, "cpu", true, error) == Result::Succeeded, "baseline checkpoint should resave after unknown field")) {
		std::filesystem::remove_all(checkpointPath);
		return false;
	}
	passed = RewriteMetadata(checkpointPath, [](json& metadata) {
		metadata["actionSpace"]["actionCount"] = ActionSpace::ActionCount() + 1;
	}) && LoadCheckpointShouldFail(checkpointPath, "metadata-incompatible") && passed;

	if (!Expect(SavePolicyValueCheckpoint(checkpointPath, model, config, 0, "cpu", true, error) == Result::Succeeded, "baseline checkpoint should resave after action mismatch")) {
		std::filesystem::remove_all(checkpointPath);
		return false;
	}
	passed = RewriteMetadata(checkpointPath, [](json& metadata) {
		metadata["parameterCount"] = metadata["parameterCount"].get<std::int64_t>() + 1;
	}) && LoadCheckpointShouldFail(checkpointPath, "metadata-incompatible") && passed;

	std::filesystem::remove_all(checkpointPath);
	return passed;
}
}

int RunPolicyValueModelTests(int argc, char* argv[]) noexcept {
	try {
		std::string deviceName = "cpu";
		for (int i = 0; i < argc; ++i) {
			const std::string argument = argv[i];
			if (argument == "--device" && i + 1 < argc) {
				deviceName = argv[++i];
			}
			else {
				std::cerr << "Unknown -test-model option: " << argument << std::endl;
				return 1;
			}
		}

		PolicyValueModelConfig tinyConfig;
		tinyConfig.hiddenChannels = 8;
		tinyConfig.residualBlocks = 1;
		tinyConfig.normGroups = 1;

		bool passed = true;
		passed = ForwardOutputHasExpectedContract(PolicyValueModelConfig(), deviceName) && passed;
		passed = ForwardOutputHasExpectedContract(tinyConfig, deviceName) && passed;
		passed = RealStandardStateRunsThroughVectorWrapper() && passed;
		passed = PolicyLayoutInvariantIsEnforced() && passed;
		passed = SeededInitializationIsDeterministic() && passed;
		passed = CheckpointRoundTripPreservesOutputs() && passed;
		passed = InvalidCheckpointMetadataIsRejected() && passed;

		if (!passed) {
			return 1;
		}

		PolicyValueModelConfig config;
		PolicyValueNetwork model = CreatePolicyValueNetwork(config, 0);
		std::cout << "Policy/value model tests passed: version=" << PolicyValueModelVersion() <<
			" hiddenChannels=" << config.hiddenChannels <<
			" residualBlocks=" << config.residualBlocks <<
			" normGroups=" << config.normGroups <<
			" input=" << StateTensor::ChannelCount() << "x" << StateTensor::BoardHeight() << "x" << StateTensor::BoardWidth() <<
			" actions=" << ActionSpace::ActionCount() <<
			" parameters=" << CountPolicyValueParameters(model) << std::endl;
		return 0;
	}
	catch (const std::exception& err) {
		std::cerr << "Policy/value model test exception: " << err.what() << std::endl;
		return 1;
	}
}
