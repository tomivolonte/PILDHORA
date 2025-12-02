const { getDefaultConfig } = require('expo/metro-config');
const path = require('path');

const config = getDefaultConfig(__dirname);

// Add resolver configuration to handle the Slider import issue
config.resolver.alias = {
  // Alias react-native Slider to @react-native-community/slider
  'react-native/Libraries/Components/Slider/Slider': '@react-native-community/slider',
  // Also handle direct import from react-native
  'react-native/Slider': '@react-native-community/slider',
};

// Add extra node modules to ensure proper resolution
config.resolver.nodeModules = [
  ...(config.resolver.nodeModules || []),
  '@react-native-community/slider',
];

// Add custom resolver to handle module resolution
config.resolver.resolveRequest = (context, moduleName, platform) => {
  // Handle the specific case of react-native-color-picker importing Slider
  if (moduleName === 'react-native/Libraries/Components/Slider/Slider' ||
      moduleName === 'react-native/Slider') {
    return context.resolveRequest(context, '@react-native-community/slider', platform);
  }
  
  // Default behavior for all other modules
  return context.resolveRequest(context, moduleName, platform);
};

// Fix MIME type issues for TypeScript and JavaScript bundles
config.server = {
  ...config.server,
  enhanceMiddleware: (middleware) => {
    return (req, res, next) => {
      // Set proper Content-Type headers for JavaScript/TypeScript bundles
      if (req.url && (req.url.endsWith('.bundle') || req.url.endsWith('.js') || req.url.endsWith('.ts'))) {
        res.setHeader('Content-Type', 'application/javascript; charset=utf-8');
      }
      // Handle JSON bundles that should be served as JavaScript
      if (req.url && req.url.includes('.bundle?') && req.headers.accept && req.headers.accept.includes('javascript')) {
        res.setHeader('Content-Type', 'application/javascript; charset=utf-8');
      }
      return middleware(req, res, next);
    };
  },
};

// Add transformer options for better compatibility
config.transformer.getTransformOptions = async () => ({
  transform: {
    experimentalImportSupport: false,
    inlineRequires: true,
  },
});

module.exports = config;