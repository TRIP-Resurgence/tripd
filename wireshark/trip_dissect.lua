trip_proto = Proto("TRIP", "Telephony Routing over IP")

trip_msg_len = ProtoField.uint16("trip.msg.len", "trip_msg_len", base.HEX)
trip_msg_type = ProtoField.uint8("trip.msg.type", "trip_msg_type", base.HEX)

trip_open_ver = ProtoField.uin8("trip.msg.open.ver", "trip_open_ver", base.HEX)
trip_open_hold = ProtoField.uin16("trip.msg.open.hold", "trip_open_hold", base.DEC)
trip_open_itad = ProtoField.uin16("trip.msg.open.itad", "trip_open_itad", base.DEC)
trip_open_id = ProtoField.uin16("trip.msg.open.id", "trip_open_id", base.HEX)
trip_open_optslen = ProtoField.uin16("trip.msg.open.opts_len", "trip_open_optslen", base.HEX)

trip_proto.fields = { trip_msg_len, trip_msg_type }

-- create a function to dissect it
function trip_proto.dissector(buffer, pinfo, tree)
    length = buffer:len()
    if length == 0 then return end

    pinfo.cols.protocol = "TRIP"
    local subtree = tree:add(trip_proto, buffer(), "TRIP data")

    -- message header
    subtree:add_le(trip_msg_len, buffer(0,2))

    local msg_type_num = buffer(2,1):le_uint()
    local msg_type_name = get_msg_type_name(type_num)
    print("type_num: " .. type_num)
    subtree:add_le(trip_msg_type, buffer(4,1)):append_text(" (" .. type_name .. ")")

    -- data
    if msg_type_num == 1 then 
        submsg_tree:add_le(trip_open_ver, buffer(3,1))
        submsg_tree:add_le(trip_open_hold, buffer(5,2))
        submsg_tree:add_le(trip_open_itad, buffer(8,4))
        submsg_tree:add_le(trip_open_id, buffer(12,4))
        submsg_tree:add_le(trip_open_optslen, buffer(16,2))
    elseif msg_type_num == 2 then 
    elseif msg_type_num == 3 then
    elseif msg_type_num == 4 then 
    end
end

function get_str_len(buffer, off)
    local string_length
    for i = off, length - 1, 1 do
      if (buffer(i,1):le_uint() == 0) then
        string_length = i - off
        break
      end
    end
    return string_length
end

function get_msg_type_name(type_num)
    local type_name = "Unknown"
  
        if type_num == 1 then type_name = "OPEN"
    elseif type_num == 2 then type_name = "UPDATE"
    elseif type_num == 3 then type_name = "NOTIFICATION"
    elseif type_num == 4 then type_name = "KEEPALIVE" end
  
    return type_name
end
  

-- load the tcp.port table
udp_table = DissectorTable.get("tcp.port")
-- register our protocol to handle udp port 6069
udp_table:add(6069,trip_proto)
