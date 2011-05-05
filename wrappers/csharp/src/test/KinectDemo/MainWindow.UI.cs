using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

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
			
			///
			/// selectVideoModeGroup
			///
			this.selectVideoModeGroup = new GroupBox();
			this.selectVideoModeGroup.Dock = DockStyle.Top;
			this.selectVideoModeGroup.Height = 150;
			this.selectVideoModeGroup.Text = "Select Modes";
			this.selectVideoModeGroup.Padding = new Padding(10);
			this.selectVideoModeGroup.Controls.Add(this.selectDepthModeCombo);
			this.selectVideoModeGroup.Controls.Add(this.selectDepthModeLabel);
			this.selectVideoModeGroup.Controls.Add(this.selectVideoModeCombo);
			this.selectVideoModeGroup.Controls.Add(this.selectVideoModeLabel);
			this.selectVideoModeGroup.Enabled = false;
			
			///
			/// refreshButton
			/// 
			this.refreshButton = new Button();
			this.refreshButton.Text = "Refresh";
			this.refreshButton.Dock = DockStyle.Fill;
			this.refreshButton.Click += HandleRefreshButtonClick;
			
			///
			/// connectButton
			/// 
			this.connectButton = new Button();
			this.connectButton.Text = "Connect";
			this.connectButton.Dock = DockStyle.Fill;
			this.connectButton.Click += HandleConnectButtonClick;
			
			///
			/// disconnectButton
			/// 
			this.disconnectButton = new Button();
			this.disconnectButton.Text = "Disconnect";
			this.disconnectButton.Dock = DockStyle.Fill;
			this.disconnectButton.Click += HandleDisconnectButtonClick;
			
			///
			/// selectDeviceControlPanel
			///
			this.selectDeviceControlsPanel = new TableLayoutPanel();
			this.selectDeviceControlsPanel.RowCount = 1;
			this.selectDeviceControlsPanel.ColumnCount = 3;
			this.selectDeviceControlsPanel.Dock = DockStyle.Top;
			this.selectDeviceControlsPanel.Height = 35;
			this.selectDeviceControlsPanel.CellBorderStyle = TableLayoutPanelCellBorderStyle.None;
			this.selectDeviceControlsPanel.Controls.Add(this.refreshButton);
			this.selectDeviceControlsPanel.Controls.Add(this.disconnectButton);
			this.selectDeviceControlsPanel.Controls.Add(this.connectButton);
			
			///
			/// selectDeviceCombo
			///
			this.selectDeviceCombo = new ComboBox();
			this.selectDeviceCombo.Dock = DockStyle.Top;
			this.selectDeviceCombo.DropDownStyle = ComboBoxStyle.DropDownList;
			
			///
			/// selectDeviceGroup
			///
			this.selectDeviceGroup = new GroupBox();
			this.selectDeviceGroup.Dock = DockStyle.Top;
			this.selectDeviceGroup.Height = 100;
			this.selectDeviceGroup.Text = "Select Device";
			this.selectDeviceGroup.Padding = new Padding(10);
			this.selectDeviceGroup.Controls.Add(this.selectDeviceControlsPanel);
			this.selectDeviceGroup.Controls.Add(this.selectDeviceCombo);
			
			///
			/// contentPanel
			///
			this.contentPanel = new Panel();
			this.contentPanel.Dock = DockStyle.Fill;
			this.contentPanel.Padding = new Padding(7);
			this.contentPanel.Controls.Add(this.selectVideoModeGroup);
			this.contentPanel.Controls.Add(this.selectDeviceGroup);
			
			///
			/// mainToolbar
			///
			this.mainToolbar = new ToolStrip();
			this.mainToolbar.Dock = DockStyle.Top;
			
			///
			/// MainWindow
			///
			this.Width = 300;
			this.Height = 400;
			this.Text = "Kinect.NET Demo";
			this.Font = new Font(this.Font.FontFamily, 9.0f);
			this.FormBorderStyle = FormBorderStyle.FixedSingle;
			this.Controls.Add(this.contentPanel);
			this.Controls.Add(this.mainToolbar);
		}
		
		/// 
		/// UI Components
		/// 
		private ToolStrip mainToolbar;
		private Panel contentPanel;
		private GroupBox selectDeviceGroup;
		private GroupBox selectVideoModeGroup;
		private ComboBox selectDeviceCombo;
		private TableLayoutPanel selectDeviceControlsPanel;
		private Button refreshButton;
		private Button connectButton;
		private Button disconnectButton;
		private ComboBox selectVideoModeCombo;
		private Label selectVideoModeLabel;
		private ComboBox selectDepthModeCombo;
		private Label selectDepthModeLabel;
	}
}