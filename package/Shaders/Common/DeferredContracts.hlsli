// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 northaxosky
#ifndef DEFERRED_CONTRACTS_HLSLI_INCLUDED
#define DEFERRED_CONTRACTS_HLSLI_INCLUDED

#define DEFERRED_PERFRAME_CB12_SHARED_BLOCK \
	row_major float4x4 WorldToView;         \
	row_major float4x4 ViewToClip;          \
	row_major float4x4 WorldToClip;         \
	float4 ViewToWorld_row0;                \
	float4 ViewToWorld_row1;                \
	float4 ViewToWorld_row2;                \
	float4 ViewToWorld_row3;                \
	row_major float4x4 ClipToWorld;         \
	float4 ClipToView_row0;                 \
	float4 ClipToView_row1;                 \
	float4 ClipToView_row2;                 \
	float4 ClipToView_row3;                 \
	float4 FirstPersonClipToView_row0;      \
	float4 FirstPersonClipToView_row1;      \
	float4 FirstPersonClipToView_row2;      \
	float4 FirstPersonClipToView_row3

#endif
