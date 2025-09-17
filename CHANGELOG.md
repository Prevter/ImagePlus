# v1.0.2
- Fixed undefined behavior when calling `initWithImageData` with invalid data/size

# v1.0.1
- Fixed unwanted texture deletion when there's only one sprite that uses it
- Replaced giflib implementation with stb_image for better compatibility
- Added setting to allow falling back to cocos2d default PNG loading

# v1.0.0
- Initial release