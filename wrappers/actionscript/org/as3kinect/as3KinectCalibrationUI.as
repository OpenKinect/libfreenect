package org.as3kinect {
    import fl.controls.Button;
    import fl.controls.Label;
    import fl.controls.Slider;
    import fl.controls.TextInput;
    import fl.events.SliderEvent;
    import flash.display.Bitmap;
    import flash.display.Sprite;
    import flash.events.Event;
    import flash.events.KeyboardEvent;
    import flash.events.MouseEvent;
    import flash.geom.Point;
    import flash.text.TextField;
    import org.as3kinect.events.as3kinectCalibrationEvent;
    import org.as3kinect.objects.as3kinectCalibrationParams;



    public class as3KinectCalibrationUI extends Sprite {
        private var _calibrationAreas : Array;
        private var _originalCoordinates : Object;
        private var _as3w : as3kinectWrapper;
        private var _bmpDepth : Bitmap;
        private var _sliceRect : Sprite;
        private var _minDistanceValueLabel : TextInput;
        private var _maxDistanceValueLabel : TextInput;
        private var _minDistanceSlider : Slider;
        private var _maxDistanceSlider : Slider;
        private var _sliceHeightValueLabel : TextInput;
        private var _sliceHeightSlider : Slider;
        private var _sliceYPosValueLabel : TextInput;
        private var _sliceYPosSlider : Slider;
        private var _calculateBtn : Button;
        private var _params : as3kinectCalibrationParams;
        private var _active : Boolean;

        public function as3KinectCalibrationUI(as3w : as3kinectWrapper) {
            _as3w = as3w;
            _calibrationAreas = [];
            _as3w.depth.compression = 100;
            _params = new as3kinectCalibrationParams();
            _originalCoordinates = [new Point(0, 0), new Point(0, 0), new Point(0, 0), new Point(0, 0)];
        }

        public function activate() : void {
            if (_active)
                return;
            _active = true;
            as3kinectUtils.calibrationParams = null;
            this.addEventListener(Event.ENTER_FRAME, update);

            // Add depth BitmapData to depth_cam MovieClip
            _bmpDepth = new Bitmap(_as3w.depth.bitmap);
            _bmpDepth.x = 300;
            _bmpDepth.y = 120;
            addChild(_bmpDepth);

            initUI();
        }

        public function deactivate() : void {
            for (var i : int = 0; i < _calibrationAreas.length; i++) {
                _calibrationAreas[i].btn.removeEventListener(MouseEvent.CLICK, calibrateBtnClickListener);
            }

            _minDistanceSlider.removeEventListener(SliderEvent.CHANGE, minDinstanceSliderChangeListener);
            _minDistanceSlider.removeEventListener(SliderEvent.THUMB_DRAG, minDinstanceSliderChangeListener);
            _minDistanceValueLabel.removeEventListener(KeyboardEvent.KEY_DOWN, minDistanceTfListener);
            _maxDistanceSlider.removeEventListener(SliderEvent.CHANGE, maxDinstanceSliderChangeListener);
            _maxDistanceSlider.removeEventListener(SliderEvent.THUMB_DRAG, maxDinstanceSliderChangeListener);
            _maxDistanceValueLabel.removeEventListener(KeyboardEvent.KEY_DOWN, maxDistanceTfListener);
            _sliceHeightSlider.removeEventListener(SliderEvent.CHANGE, sliceHeightSliderChangeListener);
            _sliceHeightSlider.removeEventListener(SliderEvent.THUMB_DRAG, sliceHeightSliderChangeListener);
            _sliceHeightValueLabel.removeEventListener(KeyboardEvent.KEY_DOWN, sliceHeightTfListener);
            _sliceYPosSlider.removeEventListener(SliderEvent.CHANGE, sliceYPosSliderChangeListener);
            _sliceYPosSlider.removeEventListener(SliderEvent.THUMB_DRAG, sliceYPosSliderChangeListener);
            _calculateBtn.removeEventListener(MouseEvent.CLICK, calculateBtnListener);
            _sliceYPosValueLabel.removeEventListener(KeyboardEvent.KEY_DOWN, sliceYPosTfListener);

            do {
                removeChildAt(0);
            } while (numChildren > 0);
            _active = false;
            _calibrationAreas = [];
        }

        private function initUI() : void {
            var rect : Sprite;
            var tf : TextField;
            var calibrateCornerBtn : Button;
            for (var i : int = 0; i < 4; i++) {
                rect = new Sprite();
                rect.visible = false;

                tf = new TextField();
                tf.visible = false;

                calibrateCornerBtn = new Button();
                calibrateCornerBtn.visible = false;
                calibrateCornerBtn.label = "calibrate corner ";
                calibrateCornerBtn.name = i.toString();
                calibrateCornerBtn.addEventListener(MouseEvent.CLICK, calibrateBtnClickListener);

                // calibration Rectangle
                with(rect.graphics) {
                    beginFill(0x00FF00);
                    if (i == 0)
                        drawRect(0, 0, 50, 50);
                    if (i == 1)
                        drawRect(stage.stageWidth - 50, 0, 50, 50);
                    if (i == 2)
                        drawRect(0, stage.stageHeight - 50, 50, 50);
                    if (i == 3)
                        drawRect(stage.stageWidth - 50, stage.stageHeight - 50, 50, 50);
                    endFill();
                }
                addChild(rect);

                // Textfield and Button
                switch (i) {
                    case 0:
                        tf.x = rect.width + 10;
                        tf.y = 20;
                        calibrateCornerBtn.move(rect.width + 10, 0);
                        break;
                    case 1:
                        tf.x = stage.stageWidth - 100 - rect.width - 10;
                        tf.y = 20;
                        calibrateCornerBtn.move(stage.stageWidth - 100 - rect.width - 10, 0);
                        break;
                    case 2:
                        tf.x = rect.width + 10;
                        tf.y = stage.stageHeight - 30;
                        calibrateCornerBtn.move(rect.width + 10, stage.stageHeight - 50);
                        break;
                    case 3:
                        tf.x = stage.stageWidth - 100 - rect.width - 10;
                        tf.y = stage.stageHeight - 30;
                        calibrateCornerBtn.move(stage.stageWidth - 100 - rect.width - 10, stage.stageHeight - 50);
                        break;
                }
                tf.text = _originalCoordinates[i];

                addChild(tf);
                addChild(calibrateCornerBtn);
                _calibrationAreas.push({rect:rect, tf:tf, btn:calibrateCornerBtn});
            }

            _sliceRect = new Sprite();
            with(_sliceRect.graphics) {
                lineStyle(1, 0xFF00FF);
                drawRect(_bmpDepth.x, _bmpDepth.y, _bmpDepth.width, _params.sliceRectHeight);
            }
            _sliceRect.y = _params.sliceRectY;
            addChild(_sliceRect);

            _minDistanceSlider = createSlider(_bmpDepth.x + _bmpDepth.width + 40, _bmpDepth.y, 200, 10, 0, 1000, 90);
            _minDistanceSlider.addEventListener(SliderEvent.CHANGE, minDinstanceSliderChangeListener);
            _minDistanceSlider.addEventListener(SliderEvent.THUMB_DRAG, minDinstanceSliderChangeListener);
            _minDistanceSlider.value = _as3w.depth.minDistance;

            var minDistanceLabel : Label = new Label();
            minDistanceLabel.x = _minDistanceSlider.x;
            minDistanceLabel.y = _minDistanceSlider.y;
            minDistanceLabel.text = "min. distance";
            addChild(minDistanceLabel);

            _minDistanceValueLabel = createInputTf(_minDistanceSlider.x - 15, _minDistanceSlider.y + _minDistanceSlider.width + 2, 30, 15, "0-9");
            _minDistanceValueLabel.text = _minDistanceSlider.value.toString();
            _minDistanceValueLabel.addEventListener(KeyboardEvent.KEY_DOWN, minDistanceTfListener);
            addChild(_minDistanceValueLabel);

            _maxDistanceSlider = createSlider(_bmpDepth.x + _bmpDepth.width + 40, _bmpDepth.y + 250, 200, 10, 0, 1000, 90);
            _maxDistanceSlider.addEventListener(SliderEvent.CHANGE, maxDinstanceSliderChangeListener);
            _maxDistanceSlider.addEventListener(SliderEvent.THUMB_DRAG, maxDinstanceSliderChangeListener);
            _maxDistanceSlider.value = _as3w.depth.maxDistance;

            var maxDistanceLabel : Label = new Label();
            maxDistanceLabel.x = _maxDistanceSlider.x;
            maxDistanceLabel.y = _maxDistanceSlider.y;
            maxDistanceLabel.text = "max. distance";
            addChild(maxDistanceLabel);

            _maxDistanceValueLabel = createInputTf(_maxDistanceSlider.x - 15, _maxDistanceSlider.y + _maxDistanceSlider.width + 2, 30, 15, "0-9");
            _maxDistanceValueLabel.text = _maxDistanceSlider.value.toString();
            _maxDistanceValueLabel.addEventListener(KeyboardEvent.KEY_DOWN, maxDistanceTfListener);
            addChild(_maxDistanceValueLabel);

            _sliceHeightSlider = createSlider(_bmpDepth.x + 100, _bmpDepth.y - 70, 300, 10, 0, 480);
            _sliceHeightSlider.addEventListener(SliderEvent.CHANGE, sliceHeightSliderChangeListener);
            _sliceHeightSlider.addEventListener(SliderEvent.THUMB_DRAG, sliceHeightSliderChangeListener);
            _sliceHeightSlider.value = _params.sliceRectHeight;

            var sliceHeightLabel : Label = new Label();
            sliceHeightLabel.x = _sliceHeightSlider.x;
            sliceHeightLabel.y = _sliceHeightSlider.y - 20;
            sliceHeightLabel.text = "slice height";
            addChild(sliceHeightLabel);

            _sliceHeightValueLabel = createInputTf(_sliceHeightSlider.x + _sliceHeightSlider.width + 8, _sliceHeightSlider.y - 6, 30, 15, "0-9");
            _sliceHeightValueLabel.text = _sliceHeightSlider.value.toString();
            _sliceHeightValueLabel.addEventListener(KeyboardEvent.KEY_DOWN, sliceHeightTfListener);

            _sliceYPosSlider = createSlider(_bmpDepth.x + 100, _bmpDepth.y - 20, 300, 0, 0, 480);
            _sliceYPosSlider.addEventListener(SliderEvent.CHANGE, sliceYPosSliderChangeListener);
            _sliceYPosSlider.addEventListener(SliderEvent.THUMB_DRAG, sliceYPosSliderChangeListener);
            _sliceYPosSlider.value = _params.sliceRectY;

            var sliceYPosLabel : Label = new Label();
            sliceYPosLabel.x = _sliceYPosSlider.x;
            sliceYPosLabel.y = _sliceYPosSlider.y - 20;
            sliceYPosLabel.text = "slice yPos";
            addChild(sliceYPosLabel);

            _sliceYPosValueLabel = createInputTf(_sliceYPosSlider.x + _sliceYPosSlider.width + 8, _sliceYPosSlider.y - 6, 30, 15, "0-9");
            _sliceYPosValueLabel.text = _sliceYPosSlider.value.toString();
            _sliceYPosValueLabel.addEventListener(KeyboardEvent.KEY_DOWN, sliceYPosTfListener);

            _calculateBtn = new Button();
            _calculateBtn.label = "next step ";
            _calculateBtn.x = stage.stageWidth / 2;
            _calculateBtn.y = stage.stageHeight - 50;
            _calculateBtn.addEventListener(MouseEvent.CLICK, calculateBtnListener);
            addChild(_calculateBtn);

            // first depth init
            _as3w.depth.minDistance = _minDistanceSlider.value;
            _as3w.depth.maxDistance = _maxDistanceSlider.value;
        }

        private function minDinstanceSliderChangeListener(event : SliderEvent) : void {
            _as3w.depth.minDistance = event.value;
            _minDistanceValueLabel.text = event.value.toString();
        }

        private function maxDinstanceSliderChangeListener(event : SliderEvent) : void {
            _as3w.depth.maxDistance = event.value;
            _maxDistanceValueLabel.text = event.value.toString();
        }

        private function sliceHeightSliderChangeListener(event : SliderEvent) : void {
            with(_sliceRect.graphics) {
                clear();
                lineStyle(1, 0xFF00FF);
                drawRect(_bmpDepth.x, _bmpDepth.y, _bmpDepth.width, event.value);
            }
            _sliceHeightValueLabel.text = event.value.toString();
        }

        private function sliceYPosSliderChangeListener(event : SliderEvent) : void {
            _sliceRect.y = event.value;
            _sliceYPosValueLabel.text = event.value.toString();
        }

        private function minDistanceTfListener(event : KeyboardEvent) : void {
            if (event.keyCode == 13) {
                _minDistanceSlider.value = Number(_minDistanceValueLabel.text);
                _as3w.depth.minDistance = _minDistanceSlider.value;
            }
        }

        private function maxDistanceTfListener(event : KeyboardEvent) : void {
            if (event.keyCode == 13) {
                _maxDistanceSlider.value = Number(_maxDistanceValueLabel.text);
                _as3w.depth.maxDistance = _maxDistanceSlider.value;
            }
        }

        private function sliceHeightTfListener(event : KeyboardEvent) : void {
            if (event.keyCode == 13) {
                with(_sliceRect.graphics) {
                    clear();
                    lineStyle(1, 0xFF00FF);
                    drawRect(_bmpDepth.x, _bmpDepth.y, _bmpDepth.width, Number(_sliceHeightValueLabel.text));
                }
                _sliceHeightSlider.value = Number(_sliceHeightValueLabel.text);
            }
        }

        private function sliceYPosTfListener(event : KeyboardEvent) : void {
            if (event.keyCode == 13) {
                _sliceRect.y = Number(_sliceYPosValueLabel.text);
                _sliceYPosSlider.value = Number(_sliceYPosValueLabel.text);
            }
        }

        private function calculateBtnListener(event : MouseEvent) : void {
            if (event.target.label == "next step ") {
                event.target.label = "calculate ";
                for (var i : int = 0; i < numChildren; i++)
                    getChildAt(i).visible = false;

                for (var j : int = 0; j < _calibrationAreas.length; j++) {
                    _calibrationAreas[j].rect.visible = true;
                    _calibrationAreas[j].tf.visible = true;
                    _calibrationAreas[j].btn.visible = true;
                }
                event.target.visible = true;

                setStepOne();

                dispatchEvent(new as3kinectCalibrationEvent(as3kinectCalibrationEvent.CALIBRATION_STEP_ONE));
            } else {
                setStepTwo();
                
                dispatchEvent(new as3kinectCalibrationEvent(as3kinectCalibrationEvent.CALIBRATION_COMPLETE));
            }
        }

        private function setStepOne() : void {
        	as3kinectUtils.calibrationParams = _params;
            _params.sliceRectY = _sliceRect.y;
            _params.sliceRectHeight = _sliceHeightSlider.value;
        }

        private function setStepTwo() : void {
            _params.screenWidth = stage.stageWidth;
            _params.screenHeight = stage.stageHeight;

            _params.yOffset = (_originalCoordinates[2].y + _originalCoordinates[3].y) / 2 ;
            var yFactor : Number = ( (_originalCoordinates[0].y + _originalCoordinates[1].y) / 2 - _params.yOffset) / 100;
            _params.yScreenFactor = int(_params.screenHeight / yFactor) / 100;

            _params.xOffsetTop = _originalCoordinates[0].x;
            var xOffsetBottom : Number = _originalCoordinates[2].x;
            _params.xOffsetDifference = xOffsetBottom - _params.xOffsetTop;

            var xFactorTop : Number = int((100 / (_originalCoordinates[1].x - _originalCoordinates[0].x)) * 100) / 100 ;
            var xFactorBottom : Number = int((100 / (_originalCoordinates[3].x - _originalCoordinates[2].x)) * 100) / 100;

            _params.xScreenFactorTop = xFactorTop * _params.screenWidth / 100;

            var xScreenFactorBottom : Number = xFactorBottom * _params.screenWidth / 100;
            _params.xScreenFactorDifference = xScreenFactorBottom - _params.xScreenFactorTop;
			_params.calibrationComplete = true;
            
        }

        private function calibrateBtnClickListener(event : MouseEvent) : void {
            if (as3kinectUtils.registeredBlobs.length > 0) {
                _calibrationAreas[int(event.target.name)].tf.text = "xPos:" + as3kinectUtils.registeredBlobs[0].x;
                _calibrationAreas[int(event.target.name)].tf.text += "\nyPos:" + as3kinectUtils.registeredBlobs[0].y * -1;

                _originalCoordinates[int(event.target.name)].x = as3kinectUtils.registeredBlobs[0].x;
                _originalCoordinates[int(event.target.name)].y = as3kinectUtils.registeredBlobs[0].y * -1;
            }
        }

        private function update(event : Event) : void {
            _as3w.depth.getBuffer();
        }

        private function createInputTf(x : Number, y : Number, w : Number, h : Number, restr : String) : TextInput {
            var inputTf : TextInput = new TextInput();
            inputTf.x = x;
            inputTf.y = y;
            inputTf.width = w;
            inputTf.height = h;
            inputTf.restrict = restr;
            addChild(inputTf);

            return inputTf;
        }

        private function createSlider(x : Number, y : Number, w : Number, h : Number, min : Number, max : Number, rot : Number = 0) : Slider {
            var slider : Slider = new Slider();
            slider.x = x;
            slider.y = y;
            slider.setSize(w, h);
            slider.minimum = min;
            slider.maximum = max;
            slider.rotation = rot;
            addChild(slider);

            return slider;
        }
    }
}
