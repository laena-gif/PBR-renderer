#include "RenderGraph.h"
#include <iostream>

void RenderGraph::AddPass(const std::string& passName, ResourceBindFunction resourceBindFunction,
	ExecuteFunction executeFunction)
{
	mRenderPasses[passName] = RenderPass{passName, resourceBindFunction, executeFunction};
}

void RenderGraph::LinkGraph()
{
	// TODO
}

void RenderGraph::Execute()
{
	// #TODO: this is not proper order, just an example
	for (auto& [passName, renderPass] : mRenderPasses)
	{
		std::cout << ">>>>> Binding resources for    " << passName << ":" << std::endl;
		renderPass.resourceBindFunction(renderPass, mRenderTargetManager);
		std::cout << "<<<<< End binding resources for" << passName << std::endl
			<< std::endl;
		
		std::cout << ">>>>> Executing node " << passName << ":" << std::endl;
		renderPass.executeFunction(renderPass);
		std::cout << "<<<<< End executing  " << passName << std::endl
			<< std::endl;
	}
}
