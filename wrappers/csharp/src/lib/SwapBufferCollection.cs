/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

using System;
using System.Runtime.InteropServices;

namespace LibFreenect
{
	/// <summary>
	/// Represents an collection of swap buffers. Data in the buffer is 
	/// safe to pass to unmanaged calls as it is free. This however means that 
	/// to prevent memory leaks, you MUST call the Dispose method after you are done 
	/// with the collection.
	/// </summary>
	/// <author>Aditya Gaddam (adityagaddam@gmail.com)</author>
	/// 
	public class SwapBufferCollection<T> : IDisposable
	{
		/// <summary>
		/// This small array takes care of mapping 
		/// where the data is and where their swapped indices are
		/// </summary>
		private int[] bufferIndicesMap;
		
		/// <summary>
		/// GC handles for each of the buffers in this collection
		/// </summary>
		private GCHandle[] bufferHandles;
		
		/// <summary>
		/// Buffers for data
		/// </summary>
		private T[][] buffers;
		
		/// <summary>
		/// Gets the buffer at the specified index
		/// </summary>
		/// <param name="index">
		/// Index of buffer to get to. Range is [0, Count - 1]
		/// </param>
		public T[] this[int index]
		{
			get
			{
				return this.buffers[this.bufferIndicesMap[index]];
			}
		}
		
		/// <summary>
		/// Gets the size of the collection
		/// </summary>
		public int Count
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Gets the size of each buffer in the collection
		/// </summary>
		public int BufferSize
		{
			get;
			private set;
		}
		
		/// <summary>
		/// Create a swap buffer collection of numBuffers size with buffers of size
		/// bufferSize
		/// </summary>
		/// <param name="numBuffers">
		/// Number of buffers to create in the collection
		/// </param>
		/// <param name="bufferSize">
		/// Size of each swap buffer
		/// </param>
		public SwapBufferCollection(int numBuffers, int bufferSize)
		{
			// Save some state
			this.BufferSize = bufferSize;
			this.Count = numBuffers;
			
			// Allocate buffers and save pinned handles to them
			this.bufferIndicesMap 	= new int[numBuffers];
			this.bufferHandles 		= new GCHandle[numBuffers];
			this.buffers 			= new T[numBuffers][];
			for(int i = 0; i < numBuffers; i++)
			{
				this.bufferIndicesMap[i] = i;
				this.buffers[i] = new T[bufferSize];
				this.bufferHandles[i] = GCHandle.Alloc(this.buffers[i], GCHandleType.Pinned);
			}
		}
		
		/// <summary>
		/// Gets the pointer to the buffer at the specified index. This is useful for passing 
		/// to unmanaged code.
		/// </summary>
		/// <param name="index">
		/// Index of the buffer to get pointer to. Range = [0, Count - 1].
		/// </param>
		/// <returns>
		/// Pointer to buffer of data
		/// </returns>
		public IntPtr GetBufferPointerAt(int index)
		{
			return this.bufferHandles[this.bufferIndicesMap[index]].AddrOfPinnedObject();
		}
		
		/// <summary>
		/// Swaps the buffers at the specified indices. After this point accessing 
		/// index1 will get you data which was previously at index2 and vice 
		/// versa. WARNING: This operation is NOT thread safe.
		/// </summary>
		/// <param name="index1">
		/// First index in the swap. Range = [0, Count - 1].
		/// </param>
		/// <param name="index2">
		/// Second index in the swap. Range = [0, Count - 1].
		/// </param>
		public void Swap(int index1, int index2)
		{
			if(index1 > this.Count - 1 || index2 > this.Count - 1)
			{
				throw new ArgumentOutOfRangeException("Indices for swapping have to be between 0 and Count - 1");
			}
			if(index1 == index2)
			{
				// Nothing to do
				return;
			}
			
			// Swap
			int tmp = this.bufferIndicesMap[index1];
			this.bufferIndicesMap[index1] = this.bufferIndicesMap[index2];
			this.bufferIndicesMap[index2] = tmp;
		}
		
		/// <summary>
		/// Disposes the buffer collection and any unmanaged resources.
		/// </summary>
		public void Dispose()
		{
			// Free all GC handles 
			for(int i = 0; i < this.Count; i++)
			{
				this.bufferHandles[i].Free();
			}
		}
		
	}
}