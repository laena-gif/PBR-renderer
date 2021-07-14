#include <iostream>
#include "RenderGraph.h"

int main()
{
	RenderGraph renderGraph;
	// adding render passes here

	renderGraph.AddPass("GBuffer",
		[](const RenderGraph::RenderPass& renderPass, RenderTargetManager& rtManager)
		{
			std::cout << "Binding resources for GBuffer here" << std::endl;
			rtManager.UseRenderTargetAsOutput("GBuffer", renderPass.passName);
		},
		[](const RenderGraph::RenderPass& renderPass)
		{
			std::cout << "Executing GBuffer here" << std::endl;
		}
	);

	renderGraph.AddPass("SSAO",
		[](const RenderGraph::RenderPass& renderPass, RenderTargetManager& rtManager)
		{
			std::cout << "Binding resources for SSAO here (should use GBuffer data here)" << std::endl;
		},
		[](const RenderGraph::RenderPass& renderPass)
		{
			std::cout << "Executing SSAO here" << std::endl;
		}
	);

	renderGraph.AddPass("ResolveLighting",
		[](const RenderGraph::RenderPass& renderPass, RenderTargetManager& rtManager)
		{
			std::cout << "Binding resources for ResolveLighting here (should use GBuffer, SSAO and shadows data here)" << std::endl;
		},
		[](const RenderGraph::RenderPass& renderPass)
		{
			std::cout << "Executing ResolveLighting here" << std::endl;
		}
	);

	renderGraph.AddPass("RenderShadows",
		[](const RenderGraph::RenderPass& renderPass, RenderTargetManager& rtManager)
		{
			std::cout << "Binding resources for Shadows here (as GBuffer, uses nothing)" << std::endl;
		},
		[](const RenderGraph::RenderPass& renderPass)
		{
			std::cout << "Executing RenderShadows here" << std::endl;
		}
	);
	
	renderGraph.LinkGraph();
	renderGraph.Execute();
}