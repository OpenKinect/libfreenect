using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;
using freenect;

namespace KinectDemo
{
	public partial class PreviewWindow : Form
	{
		/// <summary>
		/// Initialize UI components
		/// </summary>
		private void InitializeComponents()
		{	
			this.FormBorderStyle = FormBorderStyle.FixedSingle;
			this.MaximizeBox = false;
		}
	}
}