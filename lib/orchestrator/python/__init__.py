"""
Orchestrator 模块 - 管线编排引擎

功能: 组装各阶段 handler 为 PipelineStageHandlerC，串联校准→解析→PSF→光度→drizzle 全链路
"""

from .orchestrator import Orchestrator

__all__ = ["Orchestrator"]
