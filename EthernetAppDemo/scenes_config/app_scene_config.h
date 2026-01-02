/**
 * Add Scenes to work with the app scene manager
 */

// The main menu
ADD_SCENE(app, main_menu, main_menu_option)

// Sniffer Scene
ADD_SCENE(app, sniffer, sniffer_option)

// Read pcaps
ADD_SCENE(app, browser_pcaps, browser_pcaps_option)
ADD_SCENE(app, read_pcap, read_pcap_option)

// ARP Actions menu Scene
ADD_SCENE(app, arp_actions_menu, arp_action_menu_option)

// ARPSpoofing Scene
ADD_SCENE(app, arp_spoofing, arp_spoofing_option)

// ArpSpoofing a specific IP
ADD_SCENE(app, arp_spoofing_specific_ip_menu, arp_spoofing_specific_ip_menu_option)
ADD_SCENE(app, arp_spoofing_specific_ip, arp_spoofing_specific_ip_option)

// ARP scanner scene
ADD_SCENE(app, arp_scanner_menu, arp_scanner_menu_option)
ADD_SCENE(app, arp_scanner, arp_scanner_option)
ADD_SCENE(app, arp_ip_show_details, arp_ip_show_details_option)

// About US scene
ADD_SCENE(app, about_us, about_us_option)

// Testing scene
ADD_SCENE(app, testing_scene, testing_scene_option)

// Get IP scene
ADD_SCENE(app, get_ip_scene, get_ip_option)

// Ports Scanner scene
ADD_SCENE(app, ports_scanner, ports_scanner_option)

// OS Detector scene
ADD_SCENE(app, os_detector, os_detector_option)

/**
 * Scene to set the IP in the ping Option
 */
ADD_SCENE(app, ping_menu_scene, ping_menu_option)
ADD_SCENE(app, ping_set_ip_scene, ping_set_ip_option)
ADD_SCENE(app, ping_scene, ping_option)

/**
 * These scenes are for the settings
 */
ADD_SCENE(app, settings, settings_option)
ADD_SCENE(app, settings_options_menu, settings_options_menu_option)
ADD_SCENE(app, set_address, set_address_option)
