#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    auto dst = _ip_eth_table.find(next_hop_ip);
    if (dst != _ip_eth_table.end() && (_logical_time - dst->second.first) <= 30000) {
        build_push_IP_data(dgram, next_hop_ip);
    } else {
        _wait_ip_data.emplace_back(std::make_pair(next_hop_ip, dgram));
        if (_arq_req.find(next_hop_ip) == _arq_req.end()) {
            _arq_req[next_hop_ip] = _logical_time;
        } else {
            auto pre_arq = _arq_req.find(next_hop_ip);
            if (_logical_time - pre_arq->second >= 5000) {
                _arq_req[next_hop_ip] = _logical_time;
            } else {
                return;
            }
        }
        EthernetFrame frame;
        frame.header().type = EthernetHeader::TYPE_ARP;
        frame.header().src = _ethernet_address;
        frame.header().dst = ETHERNET_BROADCAST;
        ARPMessage msg;
        msg.sender_ip_address = _ip_address.ipv4_numeric();
        msg.sender_ethernet_address = _ethernet_address;
        msg.opcode = ARPMessage::OPCODE_REQUEST;
        msg.target_ip_address = next_hop_ip;
        frame.payload() = msg.serialize();
        frames_out().push(frame);
    }
}
void NetworkInterface::build_push_IP_data(const InternetDatagram &ip_data, uint32_t next_hop_ip) {
    EthernetFrame frame;
    frame.header().dst = EthernetAddress{_ip_eth_table.find(next_hop_ip)->second.second};
    frame.header().src = EthernetAddress{_ethernet_address};
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.payload() = ip_data.serialize();
    _frames_out.push(frame);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address) {
        return nullopt;
    }
    cout << "eth:" + to_string(_ethernet_address) + ",ip:" + to_string(_ip_address.ipv4_numeric()) + "receive" << frame.header().to_string() << endl;

    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram data;
        if (data.parse(frame.payload()) != ParseResult::NoError) {
            return nullopt;
        }
        return optional<InternetDatagram>{data};
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage data;
        if (data.parse(frame.payload()) != ParseResult::NoError) {
            return nullopt;
        }
        auto s_ip = data.sender_ip_address;
        auto s_eth = data.sender_ethernet_address;
        _ip_eth_table[s_ip] = make_pair(_logical_time, s_eth);
        if (data.opcode == ARPMessage::OPCODE_REQUEST && data.target_ip_address == _ip_address.ipv4_numeric()) {
            EthernetFrame reply;
            reply.header().type = EthernetHeader::TYPE_ARP;
            reply.header().src = _ethernet_address;
            reply.header().dst = data.sender_ethernet_address;
            ARPMessage msg;
            msg.sender_ip_address = _ip_address.ipv4_numeric();
            msg.sender_ethernet_address = _ethernet_address;
            msg.target_ethernet_address = data.sender_ethernet_address;
            msg.target_ip_address = data.sender_ip_address;
            msg.opcode = ARPMessage::OPCODE_REPLY;
            reply.payload() = msg.serialize();
            frames_out().push(reply);
        }
        //        if (data.opcode == ARPMessage::OPCODE_REPLY) {
        std::vector<uint32_t> arq_remove_vec{};
        for (auto i : _arq_req) {
            if (i.first == s_ip) {
                arq_remove_vec.push_back(i.first);
            }
        }
        for (auto i : arq_remove_vec) {
            _arq_req.erase(i);
        }

        for (auto it = _wait_ip_data.begin(); it != _wait_ip_data.end();) {
            if (it->first == s_ip) {
                build_push_IP_data(it->second, it->first);
                _wait_ip_data.erase(it);
            } else {
                it++;
            }
        }

        //        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { _logical_time += ms_since_last_tick; }
