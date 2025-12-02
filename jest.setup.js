import 'react-native-gesture-handler/jestSetup';

jest.mock('@expo/vector-icons', () => {
  const { View } = require('react-native');
  return {
    Ionicons: View,
    MaterialIcons: View,
    FontAwesome: View,
    Entypo: View,
    SimpleLineIcons: View,
    MaterialCommunityIcons: View,
    Feather: View,
    AntDesign: View,
  };
});

jest.mock('expo-font', () => ({
  isLoaded: jest.fn(() => true),
  loadAsync: jest.fn(() => Promise.resolve()),
}));
