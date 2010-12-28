% Copyright (c) 2010 Ryan Farrell (farrell@cs.umd.edu)
% 	    	     Brandyn White (bwhite@dappervision.com)
%
% This code is licensed to you under the terms of the Apache License, version
% 2.0, or, at your option, the terms of the GNU General Public License,
% version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
% or the following URLs:
% http://www.apache.org/licenses/LICENSE-2.0
% http://www.gnu.org/licenses/gpl-2.0.txt
%
% If you redistribute this file in source form, modified or unmodified, you
% may:
%   1) Leave this header intact and distribute it under the same terms,
%      accompanying it with the APACHE20 and GPL20 files, or
%   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
%   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
% In all cases you must keep the copyright notice intact and include a copy
% of the CONTRIB file.
%
% Binary distributions must follow the binary distribution requirements of
% either License.

classdef Freenect < handle
    % FREENECT Provides access to Microsoft Kinect using libfreenect
    %   Available methods: getFrame()
    
    properties (Constant, Hidden)
        % Note, if libfreenect is installed elsewhere, you'll need to
        % change the INSTALLED_PATH
        INSTALLED_PATH = '/usr/local/';
        
        % The LIBRARY_NAME will need to be modified for other platforms and
        % you'll also need to ensure that libfreenect.h is in the same
        % include directory
        LIBRARY_NAME = fullfile(Freenect.INSTALLED_PATH,'lib/libfreenect_sync.so');
        SYNC_HEADER  = fullfile(Freenect.INSTALLED_PATH,'include/libfreenect_sync.h');
        
        % If you'd like to see the warnigns that MATLAB returns when
        % loading the library, then enable this flag.
        SHOW_LIBRARY_WARNINGS = false;
    end

    properties (Dependent)
        cameraId;
    end
    
    properties (Access=protected)
        internalCameraId;
        
        clrBuffer;
          hClrBuffer;
        clrTStamp;
          hClrTStamp;

        depthBuffer;
          hDepthBuffer;
        depthTStamp;
          hDepthTStamp;

        lastClrPtr;
        lastClrTStamp;
        lastDepthPtr;
        lastDepthTStamp;
    end
    
    methods
        function OBJ = Freenect( CAM_ID )
            persistent gCameraMap;

            assert( nargin==1 );
            
            
            Freenect.ensureInitialized();

            
            OBJ.internalCameraId = int32(CAM_ID);
            
            OBJ.clrBuffer    = zeros(640*480*3,1,'uint8');
            OBJ.depthBuffer  = zeros(640*480,1,'uint16');
            OBJ.hClrBuffer   = libpointer('voidPtrPtr',OBJ.clrBuffer);
            OBJ.hDepthBuffer = libpointer('voidPtrPtr',OBJ.depthBuffer);
            
            OBJ.clrTStamp = zeros(1,1,'uint32');
            OBJ.depthTStamp = zeros(1,1,'uint32');
            OBJ.hClrTStamp = libpointer('uint32Ptr',OBJ.clrTStamp);
            OBJ.hDepthTStamp = libpointer('uint32Ptr',OBJ.depthTStamp);
            
                        
            if (isfield(gCameraMap,sprintf('CAM%d',CAM_ID)))
                throw(MException('Freenect:cameraTaken',...
                    sprintf('Camera ID %d Already in Use!',CAM_ID)));
            else
                % Try to get a frame, make sure it's working.
                [RGB,DEPTH] = OBJ.getFrame();
                if ( isempty(RGB) || isempty(DEPTH) )
                    throw( MException('Freenect:ConnectionError',...
                        sprintf('Couldn''t connect to camera %d',...
                        OBJ.internalCameraId)));
                end
                % Otherwise, we're good
                gCameraMap.(sprintf('CAM%d',CAM_ID)) = true;
            end
            
        end
        
        function [ID] = get.cameraId(OBJ)
            ID = OBJ.internalCameraId;
        end
        
        function [RGB,DEPTH] = getFrame(OBJ)
            % [RGB,DEPTH] = OBJ.GETFRAME() extracts the current 8-bit RGB
            % and 11-bit DEPTH images.

            ReturnClr   = OBJ.getColorPtr();
            ReturnDepth = OBJ.getDepthPtr();
            
            if ( ReturnClr == 0 )
                RGB   = permute(reshape(OBJ.lastClrPtr.Value,[3 640 480]),[3 2 1]);
            else
                RGB = [];
            end
            
            if ( ReturnDepth == 0 )
                DEPTH = reshape(OBJ.lastDepthPtr.Value,[640 480])';
            else
                DEPTH = [];
            end
        end
    end
    
    
    methods(Access=protected)
        function [RETURN] = getColorPtr(OBJ)
            VIDEO_MODE = 0;
            RawPtr = calllib('freenectAlias','freenect_sync_get_video',...
                OBJ.hClrBuffer,OBJ.hClrTStamp,...
                OBJ.internalCameraId, VIDEO_MODE);
            OBJ.lastClrPtr    = get(OBJ.hClrBuffer);
            OBJ.lastClrTStamp = get(OBJ.hClrTStamp);
            RETURN = RawPtr;
        end
        
        function [RETURN] = getDepthPtr(OBJ)
            DEPTH_MODE = 0;
            RawPtr = calllib('freenectAlias','freenect_sync_get_depth',...
                OBJ.hDepthBuffer,OBJ.hDepthTStamp,...
                OBJ.internalCameraId, DEPTH_MODE);
            OBJ.lastDepthPtr    = get(OBJ.hDepthBuffer);
            OBJ.lastDepthTStamp = get(OBJ.hDepthTStamp);
            RETURN = RawPtr;
        end
    end
    
    
    methods (Static,Access=protected)
        function [] = ensureInitialized()
            persistent gAlreadyInitialized;
            
            if ( isempty(gAlreadyInitialized) )
                
                try
                    unloadlibrary('freenectAlias');
                catch me %#ok<NASGU>
                end
                
                % First ensure library, headers exists
                OTHER_HEADERS = fullfile(Freenect.INSTALLED_PATH,'include');
                MAIN_FREENECT_HEADER = fullfile(OTHER_HEADERS,'libfreenect.h');
                if ( ~exist( Freenect.LIBRARY_NAME,'file' ) )
                    throwAsCaller(MException('Freenect:LibraryMissing',...
                        sprintf('Couldn''t find library: %s',...
                        Freenect.LIBRARY_NAME)));
                elseif ( ~exist( Freenect.SYNC_HEADER,'file' ) )
                    throwAsCaller(MException('Freenect:HeaderMissing',...
                        sprintf('Couldn''t find header: %s',...
                        Freenect.SYNC_HEADER)));
                elseif ( ~exist( MAIN_FREENECT_HEADER,'file' ) )
                    throwAsCaller(MException('Freenect:HeaderMissing',...
                        sprintf('Couldn''t find header: %s',...
                        MAIN_FREENECT_HEADER)));
                end
                
                [~,warnings] = loadlibrary( ...
                    Freenect.LIBRARY_NAME, ...
                    Freenect.SYNC_HEADER, ...
                    'alias', 'freenectAlias', ...
                    'includepath', OTHER_HEADERS );
                
                if ( Freenect.SHOW_LIBRARY_WARNINGS )
                    fprintf(2,warnings);
                end
                                
                gAlreadyInitialized = true;
            end
            

        end
        
        function [] = shutdown()
            % call the freenect shutdown() here - TODO
            % currently nothing calls shutdown()...
            unloadlibrary('freenectAlias');
        end
    end
end
