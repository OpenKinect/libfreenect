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
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;
using freenect;

namespace KinectDemo
{
	public partial class MainWindow : Form
	{
		
		/// <summary>
		/// Initialize UI components
		/// </summary>
		private void InitializeComponents()
		{
			
			///
			/// accelerometerZValueLabel
			///
			this.accelerometerZValueLabel = new Label();
			this.accelerometerZValueLabel.Text = "0";
			this.accelerometerZValueLabel.Dock = DockStyle.Fill;
			
			///
			/// accelerometerYValueLabel
			///
			this.accelerometerYValueLabel = new Label();
			this.accelerometerYValueLabel.Text = "0";
			this.accelerometerYValueLabel.Dock = DockStyle.Fill;
			
			///
			/// accelerometerXValueLabel
			///
			this.accelerometerXValueLabel = new Label();
			this.accelerometerXValueLabel.Text = "0";
			this.accelerometerXValueLabel.Dock = DockStyle.Fill;
			
			///
			/// accelerometerZHeaderLabel
			///
			this.accelerometerZHeaderLabel = new Label();
			this.accelerometerZHeaderLabel.Text = "Z:";
			this.accelerometerZHeaderLabel.Dock = DockStyle.Fill;
			this.accelerometerZHeaderLabel.Font = new Font(this.Font.FontFamily, this.Font.Size, FontStyle.Bold);
			
			///
			/// accelerometerYHeaderLabel
			///
			this.accelerometerYHeaderLabel = new Label();
			this.accelerometerYHeaderLabel.Text = "Y:";
			this.accelerometerYHeaderLabel.Dock = DockStyle.Fill;
			this.accelerometerYHeaderLabel.Font = new Font(this.Font.FontFamily, this.Font.Size, FontStyle.Bold);
			
			///
			/// accelerometerXHeaderLabel
			///
			this.accelerometerXHeaderLabel = new Label();
			this.accelerometerXHeaderLabel.Text = "X:";
			this.accelerometerXHeaderLabel.Dock = DockStyle.Fill;
			this.accelerometerXHeaderLabel.Font = new Font(this.Font.FontFamily, this.Font.Size, FontStyle.Bold);
			
			///
			/// accelerometerStatusTable
			/// 
			this.accelerometerStatusTable = new TableLayoutPanel();
			this.accelerometerStatusTable.RowCount = 3;
			this.accelerometerStatusTable.ColumnCount = 2;
			this.accelerometerStatusTable.Dock = DockStyle.Fill;
			this.accelerometerStatusTable.Controls.Add(this.accelerometerXHeaderLabel, 0, 0);
			this.accelerometerStatusTable.Controls.Add(this.accelerometerXValueLabel, 1, 0);
			this.accelerometerStatusTable.Controls.Add(this.accelerometerYHeaderLabel, 0, 1);
			this.accelerometerStatusTable.Controls.Add(this.accelerometerYValueLabel, 1, 1);
			this.accelerometerStatusTable.Controls.Add(this.accelerometerZHeaderLabel, 0, 2);
			this.accelerometerStatusTable.Controls.Add(this.accelerometerZValueLabel, 1, 2);
			
			///
			/// accelerometerStatusGroup
			///
			this.accelerometerStatusGroup = new GroupBox();
			this.accelerometerStatusGroup.Dock = DockStyle.Fill;
			this.accelerometerStatusGroup.Text = "Accelerometer";
			this.accelerometerStatusGroup.Height = 100;
			this.accelerometerStatusGroup.Padding = new Padding(10);
			this.accelerometerStatusGroup.Controls.Add(this.accelerometerStatusTable);
			
			///
			/// selectLEDColorCombo
			///
			this.selectLEDColorCombo = new ComboBox();
			this.selectLEDColorCombo.Dock = DockStyle.Fill;
			this.selectLEDColorCombo.DropDownStyle = ComboBoxStyle.DropDownList;
			string[] colors = Enum.GetNames(typeof(LEDColor));
			for(int i = 0; i < colors.Length; i++)
			{
				this.selectLEDColorCombo.Items.Add(colors[i]);
			}
			if(colors.Length > 0)
			{
				this.selectLEDColorCombo.SelectedIndex = 0;
			}
			this.selectLEDColorCombo.SelectedIndexChanged += HandleSelectLEDColorComboSelectedIndexChanged;
			
			///
			/// ledControlGroup
			///
			this.ledControlGroup = new GroupBox();
			this.ledControlGroup.Dock = DockStyle.Left;
			this.ledControlGroup.Text = "LED";
			this.ledControlGroup.Height = 60;
			this.ledControlGroup.Padding = new Padding(10);
			this.ledControlGroup.Controls.Add(this.selectLEDColorCombo);
			
			///
			/// motorCurrentTiltLabel
			///
			this.motorCurrentTiltLabel = new Label();
			this.motorCurrentTiltLabel.Dock = DockStyle.Top;
			this.motorCurrentTiltLabel.TextAlign = ContentAlignment.MiddleLeft;
			this.motorCurrentTiltLabel.AutoSize = false;
			this.motorCurrentTiltLabel.Height = 25;
			this.motorCurrentTiltLabel.Text = "Current Tilt:";
			
			///
			/// motorTiltStatusLabel
			///
			this.motorTiltStatusLabel = new Label();
			this.motorTiltStatusLabel.Dock = DockStyle.Top;
			this.motorTiltStatusLabel.TextAlign = ContentAlignment.MiddleLeft;
			this.motorTiltStatusLabel.AutoSize = false;
			this.motorTiltStatusLabel.Height = 25;
			this.motorTiltStatusLabel.Text = "Tilt Status:";
			
			///
			/// motorTiltUpDown
			/// 
			this.motorTiltUpDown = new NumericUpDown();
			this.motorTiltUpDown.Dock = DockStyle.Top;
			this.motorTiltUpDown.AutoSize = false;
			this.motorTiltUpDown.Height = 35;
			this.motorTiltUpDown.DecimalPlaces = 1;
			this.motorTiltUpDown.Minimum = -1.0m;
			this.motorTiltUpDown.Maximum = 1.0m;
			this.motorTiltUpDown.Increment = 0.1m;
			this.motorTiltUpDown.ValueChanged += HandleMotorTiltUpDownValueChanged;
			
			///
			/// motorControlGroup
			///
			this.motorControlGroup = new GroupBox();
			this.motorControlGroup.Dock = DockStyle.Left;
			this.motorControlGroup.Text = "Motor";
			this.motorControlGroup.Height = 105;
			this.motorControlGroup.Padding = new Padding(10);
			this.motorControlGroup.Controls.Add(this.motorTiltStatusLabel);
			this.motorControlGroup.Controls.Add(this.motorCurrentTiltLabel);
			this.motorControlGroup.Controls.Add(this.motorTiltUpDown);
			
			///
			/// selectDepthModeLabel
			///
			this.selectDepthModeLabel = new Label();
			this.selectDepthModeLabel.Text = "Depth Mode:";
			this.selectDepthModeLabel.Dock = DockStyle.Top;
			this.selectDepthModeLabel.TextAlign = ContentAlignment.MiddleLeft;
			
			///
			/// selectDepthModeCombo
			///
			this.selectDepthModeCombo = new ComboBox();
			this.selectDepthModeCombo.Dock = DockStyle.Top;
			this.selectDepthModeCombo.DropDownStyle = ComboBoxStyle.DropDownList;
			this.selectDepthModeCombo.SelectedIndexChanged += HandleSelectDepthModeComboSelectedIndexChanged;
			
			///
			/// selectVideoModeLabel
			///
			this.selectVideoModeLabel = new Label();
			this.selectVideoModeLabel.Text = "Video Mode:";
			this.selectVideoModeLabel.Dock = DockStyle.Top;
			this.selectVideoModeLabel.TextAlign = ContentAlignment.MiddleLeft;
			
			///
			/// selectVideoModeCombo
			///
			this.selectVideoModeCombo = new ComboBox();
			this.selectVideoModeCombo.Dock = DockStyle.Top;
			this.selectVideoModeCombo.DropDownStyle = ComboBoxStyle.DropDownList;
			this.selectVideoModeCombo.SelectedIndexChanged += HandleSelectVideoModeComboSelectedIndexChanged;;
			
			///
			/// selectVideoModeGroup
			///
			this.selectVideoModeGroup = new GroupBox();
			this.selectVideoModeGroup.Dock = DockStyle.Left;
			this.selectVideoModeGroup.Height = 130;
			this.selectVideoModeGroup.Text = "Select Modes";
			this.selectVideoModeGroup.Padding = new Padding(10);
			this.selectVideoModeGroup.Controls.Add(this.selectDepthModeCombo);
			this.selectVideoModeGroup.Controls.Add(this.selectDepthModeLabel);
			this.selectVideoModeGroup.Controls.Add(this.selectVideoModeCombo);
			this.selectVideoModeGroup.Controls.Add(this.selectVideoModeLabel);
			this.selectVideoModeGroup.Enabled = false;
			
			///
			/// controlsPanel
			///
			this.controlsPanel = new Panel();
			this.controlsPanel.Dock = DockStyle.Bottom;
			this.controlsPanel.Padding = new Padding(7);
			this.controlsPanel.Height = 150;
			this.controlsPanel.Controls.Add(this.accelerometerStatusGroup);
			this.controlsPanel.Controls.Add(this.ledControlGroup);
			this.controlsPanel.Controls.Add(this.motorControlGroup);
			this.controlsPanel.Controls.Add(this.selectVideoModeGroup);
			
			///
			/// previewControl
			///
			this.previewControl = new PreviewControl();
			this.previewControl.Dock = DockStyle.Fill;
			
			///
			/// contentPanel
			/// 
			this.contentPanel = new Panel();
			this.contentPanel.Dock = DockStyle.Fill;
			this.contentPanel.Controls.Add(this.previewControl);
			this.contentPanel.Controls.Add(this.controlsPanel);
			
			///
			/// aboutButton
			///
			this.aboutButton = new ToolStripButton();
			this.aboutButton.Text = "About";
			this.aboutButton.Padding = new Padding(7, 0, 7, 0);
			this.aboutButton.Margin = new Padding(10, 7, 0, 7);
			this.aboutButton.Click += HandleAboutButtonClick;
			
			///
			/// disconnectButton
			/// 
			this.disconnectButton = new ToolStripButton();
			this.disconnectButton.Text = "Disconnect";
			this.disconnectButton.Padding = new Padding(7, 0, 7, 0);
			this.disconnectButton.Margin = new Padding(10, 7, 0, 7);
			this.disconnectButton.Visible = false;
			this.disconnectButton.Click += HandleDisconnectButtonClick;
			
			///
			/// refreshButton
			/// 
			this.refreshButton = new ToolStripButton();
			this.refreshButton.Text = "Refresh";
			this.refreshButton.Padding = new Padding(7, 0, 7, 0);
			this.refreshButton.Margin = new Padding(10, 7, 0, 7);
			this.refreshButton.Click += HandleRefreshButtonClick;
			
			///
			/// connectButton
			/// 
			this.connectButton = new ToolStripButton();
			this.connectButton.Text = "Connect";
			this.connectButton.Padding = new Padding(7, 0, 7, 0);
			this.connectButton.Margin = new Padding(10, 7, 0, 7);
			this.connectButton.Click += HandleConnectButtonClick;
			
			///
			/// selectDeviceCombo
			///
			this.selectDeviceCombo = new ToolStripComboBox();
			this.selectDeviceCombo.Width = 150;
			this.selectDeviceCombo.Margin = new Padding(7, 0, 0, 0);
			this.selectDeviceCombo.DropDownStyle = ComboBoxStyle.DropDownList;
			
			///
			/// mainToolbar
			///
			this.mainToolbar = new ToolStrip();
			this.mainToolbar.Dock = DockStyle.Top;
			this.mainToolbar.GripStyle = ToolStripGripStyle.Hidden;
			this.mainToolbar.AutoSize = false;
			this.mainToolbar.Height = 35;
			this.mainToolbar.Items.Add(this.selectDeviceCombo);
			this.mainToolbar.Items.Add(this.refreshButton);
			this.mainToolbar.Items.Add(this.connectButton);
			this.mainToolbar.Items.Add(this.disconnectButton);
			this.mainToolbar.Items.Add(this.aboutButton);
			
			///
			/// MainWindow
			///
			this.Width = 1280;
			this.Height = 630;
			this.Text = "Kinect.NET Demo";
			this.Font = new Font(this.Font.FontFamily, 9.0f);
			this.Controls.Add(this.contentPanel);
			this.Controls.Add(this.mainToolbar);
			this.FormClosing += HandleFormClosing;
		}
		
		/// 
		/// UI Components
		///
		private ToolStrip mainToolbar;
		private Panel controlsPanel;
		private Panel contentPanel;
		
		private PreviewControl previewControl;
		
		private GroupBox motorControlGroup;
		private NumericUpDown motorTiltUpDown;
		private Label motorCurrentTiltLabel;
		private Label motorTiltStatusLabel;
		
		private GroupBox accelerometerStatusGroup;
		private TableLayoutPanel accelerometerStatusTable;
		private Label accelerometerXHeaderLabel;
		private Label accelerometerYHeaderLabel;
		private Label accelerometerZHeaderLabel;
		private Label accelerometerXValueLabel;
		private Label accelerometerYValueLabel;
		private Label accelerometerZValueLabel;
		
		private GroupBox ledControlGroup;
		private ComboBox selectLEDColorCombo;
		
		private ToolStripComboBox selectDeviceCombo;
		private ToolStripButton refreshButton;
		private ToolStripButton connectButton;
		private ToolStripButton disconnectButton;
		private ToolStripButton aboutButton;
		
		private GroupBox selectVideoModeGroup;
		private ComboBox selectVideoModeCombo;
		private Label selectVideoModeLabel;
		private ComboBox selectDepthModeCombo;
		private Label selectDepthModeLabel;
	}
}