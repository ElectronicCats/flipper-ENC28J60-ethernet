/**
 * Add Scenes to work with the app scene manager
 */

// The main menu
ADD_SCENE(app, main_menu, main_menu_option)

// Sniffer Scene
ADD_SCENE(app, sniffer, sniffer_option)

// Read pcaps
ADD_SCENE(app, read_pcap, read_pcap_option)

// ARPSpoofing Scene
ADD_SCENE(app, arp_spoofing, arp_spoofing_option)

// ARP scanner scene
ADD_SCENE(app, arp_scanner_menu, arp_scanner_menu_option)
ADD_SCENE(app, arp_scanner, arp_scanner_option)

// Testing scene
ADD_SCENE(app, testing_scene, testing_scene_option)

/**
 * These scenes are for the settings
 */
ADD_SCENE(app, settings, settings_option)
ADD_SCENE(app, settings_options_menu, settings_options_menu_option)
ADD_SCENE(app, set_address, set_address_option)
