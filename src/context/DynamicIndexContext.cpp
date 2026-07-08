#include "DynamicIndexContext.h"


// void ceateRootSignatureForDynamicIndexing()
// {
//     CD3DX12_DESCRIPTOR_RANGE texTable;
// texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0);
// // Root parameter can be a table, root descriptor or root constants.
// CD3DX12_ROOT_PARAMETER slotRootParameter[4];
// // Perfomance TIP: Order from most frequent to least frequent.
// slotRootParameter[0].InitAsConstantBufferView(0);
// slotRootParameter[1].InitAsConstantBufferView(1);
// slotRootParameter[2].InitAsShaderResourceView(0, 1);
// slotRootParameter[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
// auto staticSamplers = GetStaticSamplers();
// // A root signature is an array of root parameters.
// CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
// (UINT)staticSamplers.size(), staticSamplers.data(),
// D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

// }

// /*
// Now, before we draw any render-items, we can bind all of our materials and
// texture SRVs once per frame rather than per-render-item, and then each renderitem
// just sets the object constant buffer:
// */

// void draw()
// {
    
// auto passCB = mCurrFrameResource->PassCB->Resource();
// mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());
// // Bind all the materials used in this scene. For structured buffers,
// // we can bypass the heap and set as a root descriptor.
// auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
// mCommandList->SetGraphicsRootShaderResourceView(2,matBuffer->GetGPUVirtualAddress());
// // Bind all the textures used in this scene. Observe
// // that we only have to specify the first descriptor in the table.
// // The root signature knows how many descriptors are expected in the table.
// mCommandList->SetGraphicsRootDescriptorTable(3,mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
// DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

// }

// void DrawRenderItems( ID3D12GraphicsCommandList* cmdList,const std::vector<RenderItem*>& ritems)
// {
// // For each render item...
// for(size_t i = 0; i < ritems.size(); ++i)
// {
// auto ri = ritems[i];
// ...
// cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
// cmdList->DrawIndexedInstanced(ri->IndexCount, 1,
// ri->StartIndexLocation, ri->BaseVertexLocation, 0);
// }
