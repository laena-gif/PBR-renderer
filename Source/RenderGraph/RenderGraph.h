#pragma once
#include <string>
#include <functional>
#include <map>
#include <unordered_map>

class RenderTargetManager
{
public:
	using RenderTargetId = uint32_t;

	RenderTargetId UseRenderTargetAsInput(const std::string& name, const std::string& renderPassName) { return 1; };
	RenderTargetId UseRenderTargetAsOutput(const std::string& name, const std::string& renderPassName) { return 1; };
};

class RenderGraph
{
public:
	struct RenderPass;
	
	using ResourceBindFunction = std::function<void(const RenderPass& renderPass, RenderTargetManager& rtManager)>;
	using ExecuteFunction = std::function<void(const RenderPass& renderPass)>;

	struct RenderPass
	{
		std::string passName;
		ResourceBindFunction resourceBindFunction;
		ExecuteFunction executeFunction;
	};
	
	void AddPass(const std::string& passName, ResourceBindFunction resourceBindFunction, ExecuteFunction executeFunction);
	void LinkGraph();
	void Execute();

private:
	std::unordered_map<std::string, RenderPass> mRenderPasses;
	RenderTargetManager mRenderTargetManager;
};