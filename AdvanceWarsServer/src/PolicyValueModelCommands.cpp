#include "PolicyValueModelCommands.h"

#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

#include "ActionSpace.h"
#include "PolicyValueModel.h"
#include "StateTensor.h"

namespace {
bool ParseIntOption(const std::string& value, int& parsed) {
	try {
		std::size_t index = 0;
		const long long raw = std::stoll(value, &index, 10);
		if (index != value.size() || raw < std::numeric_limits<int>::min() || raw > std::numeric_limits<int>::max()) {
			return false;
		}
		parsed = static_cast<int>(raw);
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

bool ParseSeedOption(const std::string& value, std::int64_t& parsed) {
	try {
		std::size_t index = 0;
		const long long raw = std::stoll(value, &index, 10);
		if (index != value.size() || raw < 0) {
			return false;
		}
		parsed = static_cast<std::int64_t>(raw);
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

torch::Tensor MakeModelCommandSmokeInput() {
	return torch::zeros(
		{ 1, StateTensor::ChannelCount(), StateTensor::BoardHeight(), StateTensor::BoardWidth() },
		torch::TensorOptions().dtype(torch::kFloat32)).contiguous();
}

int PrintModelError(const PolicyValueModelError& error) {
	std::cerr << error.code << ": " << error.message << std::endl;
	return 1;
}

int RunCheckpointValidation(const std::filesystem::path& checkpointDir, const torch::Device& requestedDevice, PolicyValueModelError& error) {
	PolicyValueNetwork loaded(nullptr);
	PolicyValueModelConfig loadedConfig;
	std::int64_t loadedSeed = -1;
	if (LoadPolicyValueCheckpoint(checkpointDir, loaded, loadedConfig, loadedSeed, error) == Result::Failed) {
		return PrintModelError(error);
	}

	PolicyValueNetworkOutput output;
	if (RunPolicyValueInference(loaded, MakeModelCommandSmokeInput(), output, error) == Result::Failed) {
		return PrintModelError(error);
	}

	if (requestedDevice.is_cuda()) {
		loaded->to(requestedDevice);
		if (RunPolicyValueInference(loaded, MakeModelCommandSmokeInput().to(requestedDevice), output, error) == Result::Failed) {
			return PrintModelError(error);
		}
	}

	std::cout << "Checkpoint ready: " << checkpointDir.string() <<
		" version=" << PolicyValueModelVersion() <<
		" hiddenChannels=" << loadedConfig.hiddenChannels <<
		" residualBlocks=" << loadedConfig.residualBlocks <<
		" normGroups=" << loadedConfig.normGroups <<
		" actions=" << ActionSpace::ActionCount() <<
		" seed=" << loadedSeed <<
		" validatedDevice=" << PolicyValueDeviceName(requestedDevice) << std::endl;
	return 0;
}

int RunMnistExperiment(const std::filesystem::path& mnistPath) {
	struct Net : torch::nn::Module {
		Net()
			: conv1(torch::nn::Conv2dOptions(1, 32, 5)),
			conv2(torch::nn::Conv2dOptions(32, 64, 5)),
			fc1(1024, 128),
			fc2(128, 10) {
			register_module("conv1", conv1);
			register_module("conv2", conv2);
			register_module("fc1", fc1);
			register_module("fc2", fc2);
		}

		torch::Tensor forward(torch::Tensor x) {
			x = torch::relu(conv1->forward(x));
			x = torch::max_pool2d(x, 2);
			x = torch::relu(conv2->forward(x));
			x = torch::max_pool2d(x, 2);
			x = x.view({ x.size(0), -1 });
			x = torch::relu(fc1->forward(x));
			x = fc2->forward(x);
			return torch::log_softmax(x, 1);
		}

		torch::nn::Conv2d conv1, conv2;
		torch::nn::Linear fc1, fc2;
	};

	torch::Device device(PolicyValueCudaAvailable() ? torch::kCUDA : torch::kCPU);
	std::cout << "Using device: " << (device.is_cuda() ? "CUDA" : "CPU") << "\n";

	auto trainDataset = torch::data::datasets::MNIST(mnistPath.string()).map(
		torch::data::transforms::Stack<>());
	auto trainLoader = torch::data::make_data_loader(std::move(trainDataset),
		torch::data::DataLoaderOptions().batch_size(64));

	Net model;
	model.to(device);
	torch::optim::SGD optimizer(model.parameters(), torch::optim::SGDOptions(0.01).momentum(0.9));

	for (size_t epoch = 1; epoch <= 5; ++epoch) {
		size_t batchIndex = 0;
		for (auto& batch : *trainLoader) {
			model.train();
			auto data = batch.data.to(device);
			auto targets = batch.target.to(device);
			optimizer.zero_grad();
			auto output = model.forward(data);
			auto loss = torch::nll_loss(output, targets);
			loss.backward();
			optimizer.step();

			if (batchIndex++ % 100 == 0) {
				std::cout << "Train Epoch: " << epoch << " [" << batchIndex * 64
					<< "/60000] Loss: " << loss.item<float>() << "\n";
			}
		}
	}
	return 0;
}
}

int RunModelInitCommand(int argc, char* argv[]) noexcept {
	try {
		std::filesystem::path outputPath;
		std::string deviceName = "cpu";
		PolicyValueModelConfig config;
		std::int64_t seed = 0;
		bool force = false;

		for (int i = 0; i < argc; ++i) {
			const std::string argument = argv[i];
			if (argument == "--out" && i + 1 < argc) {
				outputPath = argv[++i];
			}
			else if (argument == "--device" && i + 1 < argc) {
				deviceName = argv[++i];
			}
			else if (argument == "--hidden-channels" && i + 1 < argc) {
				if (!ParseIntOption(argv[++i], config.hiddenChannels)) {
					std::cerr << "Invalid --hidden-channels value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--res-blocks" && i + 1 < argc) {
				if (!ParseIntOption(argv[++i], config.residualBlocks)) {
					std::cerr << "Invalid --res-blocks value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--norm-groups" && i + 1 < argc) {
				if (!ParseIntOption(argv[++i], config.normGroups)) {
					std::cerr << "Invalid --norm-groups value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--seed" && i + 1 < argc) {
				if (!ParseSeedOption(argv[++i], seed)) {
					std::cerr << "Invalid --seed value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--force") {
				force = true;
			}
			else {
				std::cerr << "Unknown -model-init option: " << argument << std::endl;
				return 1;
			}
		}

		if (outputPath.empty()) {
			std::cerr << "-model-init requires --out <checkpoint-dir>" << std::endl;
			return 1;
		}

		PolicyValueModelError error;
		const torch::Device requestedDevice = ResolvePolicyValueDevice(deviceName, error);
		if (!error.code.empty()) {
			return PrintModelError(error);
		}
		if (ValidatePolicyValueModelConfig(config, error) == Result::Failed) {
			return PrintModelError(error);
		}

		PolicyValueNetwork model = CreatePolicyValueNetwork(config, seed);
		if (SavePolicyValueCheckpoint(outputPath, model, config, seed, PolicyValueDeviceName(requestedDevice), force, error) == Result::Failed) {
			return PrintModelError(error);
		}

		return RunCheckpointValidation(outputPath, requestedDevice, error);
	}
	catch (const std::exception& err) {
		std::cerr << "model init exception: " << err.what() << std::endl;
		return 1;
	}
}

int RunTorchLibCommand(int argc, char* argv[]) noexcept {
	try {
		std::filesystem::path mnistPath;
		for (int i = 0; i < argc; ++i) {
			const std::string argument = argv[i];
			if (argument == "--mnist-path" && i + 1 < argc) {
				mnistPath = argv[++i];
			}
			else {
				std::cerr << "Unknown -torchlib option: " << argument << std::endl;
				return 1;
			}
		}

		std::cout << "CUDA support: " << (PolicyValueCudaAvailable() ? "true" : "false") << std::endl;
		std::cout << "CUDA devices: " << PolicyValueCudaDeviceCount() << std::endl;
		torch::Tensor tensor = torch::rand({ 2, 3 });
		std::cout << tensor << std::endl;

		if (mnistPath.empty()) {
			std::cout << "MNIST experiment skipped; pass --mnist-path <path> to run it." << std::endl;
			return 0;
		}

		return RunMnistExperiment(mnistPath);
	}
	catch (const std::exception& err) {
		std::cerr << "torchlib exception: " << err.what() << std::endl;
		return 1;
	}
}
