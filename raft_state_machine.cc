#include "raft_state_machine.h"
void encodeNum(char *buf, const int &value)
{
    assert(sizeof(int) == 4);
    buf[0] = (value >> 24) & 0xff;
    buf[1] = (value >> 16) & 0xff;
    buf[2] = (value >> 8) & 0xff;
    buf[3] = value & 0xff;
}
void decodeNum(const char *buf, int &value)
{
    assert(sizeof(int) == 4);
    value = (buf[0] & 0xff) << 24;
    value |= (buf[1] & 0xff) << 16;
    value |= (buf[2] & 0xff) << 8;
    value |= buf[3] & 0xff;
}
kv_command::kv_command() : kv_command(CMD_NONE, "", "") {}

kv_command::kv_command(command_type tp, const std::string &key, const std::string &value) : cmd_tp(tp), key(key), value(value), res(std::make_shared<result>())
{
    res->start = std::chrono::system_clock::now();
    res->key = key;
}

kv_command::kv_command(const kv_command &cmd) : cmd_tp(cmd.cmd_tp), key(cmd.key), value(cmd.value), res(cmd.res) {}

kv_command::~kv_command() {}

int kv_command::size() const
{
    // Your code here:
    if (cmd_tp == CMD_NONE)
    {
        return 0;
    }
    int ret = static_cast<int>(3 * sizeof(int) + key.size() + value.size());
    return ret;
}

void kv_command::serialize(char *buf, int size) const
{
    if (size != this->size() || cmd_tp == CMD_NONE)
    {
        return;
    }

    int pos = 0;
    encodeNum((buf + pos), (int)cmd_tp);
    pos += sizeof(int);
    encodeNum((buf + pos), key.size());
    pos += sizeof(int);
    encodeNum((buf + pos), value.size());
    pos += sizeof(int);
    memcpy((buf + pos), key.c_str(), key.size());
    pos += key.size();
    memcpy((buf + pos), value.c_str(), value.size());
    pos += value.size();
    assert(pos == size);
    return;
}

void kv_command::deserialize(const char *buf, int size)
{
    // Your code here:
    if (size == 0)
    {
        return;
    }
    int key_size, value_size;

    int pos = 0, cmd_int_tp = 0;
    decodeNum((buf + pos), cmd_int_tp);
    cmd_tp = (command_type)cmd_int_tp;
    pos += sizeof(int);
    decodeNum((buf + pos), key_size);
    pos += sizeof(int);
    decodeNum((buf + pos), value_size);
    pos += sizeof(int);

    char *key_array = new char[key_size], *value_array = new char[value_size];
    memcpy(key_array, (buf + pos), key_size);
    pos += key_size;
    memcpy(value_array, (buf + pos), value_size);
    pos += value_size;

    key = std::string(key_array, key_size);
    value = std::string(value_array, value_size);
    delete[] key_array;
    delete[] value_array;

    return;
}

void kv_command::set_command_type(int type)
{
    cmd_tp = (command_type)type;
}

marshall &operator<<(marshall &m, const kv_command &cmd)
{
    // Your code here:
    m << (int)cmd.cmd_tp << cmd.key << cmd.value;
    return m;
}

unmarshall &operator>>(unmarshall &u, kv_command &cmd)
{
    // Your code here:
    int tp;
    u >> tp >> cmd.key >> cmd.value;
    cmd.set_command_type(tp);
    return u;
}

kv_state_machine::~kv_state_machine()
{
}

std::vector<char> kv_state_machine::snapshot()
{
    // Your code here:
    mtx.lock();
    int snapshot_size = sizeof(int), n = store.size();
    for (auto &s : store)
    {
        snapshot_size += 2 * sizeof(int);
        snapshot_size += s.first.size();
        snapshot_size += s.second.size();
    }
    char *arr = new char[snapshot_size];
    int pos = 0;
    encodeNum((arr + pos), n);
    pos += sizeof(int);

    for (auto &s : store)
    {
        encodeNum((arr + pos), s.first.size());
        pos += sizeof(int);
        encodeNum((arr + pos), s.second.size());
        pos += sizeof(int);
        s.first.copy((arr + pos), s.first.size());
        pos += s.first.size();
        s.second.copy((arr + pos), s.second.size());
        pos += s.second.size();
    }
    assert(pos == snapshot_size);

    mtx.unlock();
    std::string data(arr, snapshot_size);
    return std::vector<char>(data.begin(), data.end());
}

void kv_state_machine::apply_snapshot(const std::vector<char> &snapshot)
{
    // Your code here:
    mtx.lock();
    std::string s(snapshot.begin(), snapshot.end());
    auto ptr = s.c_str();
    int snapshot_size, pos = 0;
    decodeNum(ptr + pos, snapshot_size);
    pos += sizeof(int);
    for (int i = 0; i < snapshot_size; ++i)
    {
        int key_s, value_s;
        decodeNum((ptr + pos), key_s);
        pos += sizeof(int);
        decodeNum((ptr + pos), value_s);
        pos += sizeof(int);
        char *k = new char[key_s], *v = new char[value_s];
        memcpy(k, (ptr + pos), key_s);
        pos += key_s;
        memcpy(v, (ptr + pos), value_s);
        pos += value_s;
        std::string key(k, key_s), value(v, value_s);
        store.insert({key, value});
    }
    mtx.unlock();
    return;
}

void kv_state_machine::apply_log(raft_command &cmd)
{
    kv_command &kv_cmd = dynamic_cast<kv_command &>(cmd);
    std::unique_lock<std::mutex> lock(kv_cmd.res->mtx);
    mtx.lock();
    // Your code here:
    switch (kv_cmd.cmd_tp){
    case kv_command::CMD_NONE:
        break;
    case kv_command::CMD_GET:
        if (store.count(kv_cmd.key))
        {
            kv_cmd.res->succ = true;
            kv_cmd.res->key = kv_cmd.key;
            kv_cmd.res->value = store[kv_cmd.key];
        }
        else
        {
            kv_cmd.res->succ = false;
            kv_cmd.res->key = kv_cmd.key;
            kv_cmd.res->value = "";
        }
        break;
    case kv_command::CMD_DEL:
        if (store.count(kv_cmd.key))
        {
            kv_cmd.res->succ = true;
            kv_cmd.res->key = kv_cmd.key;
            kv_cmd.res->value = store[kv_cmd.key];
            store.erase(kv_cmd.key);
        }
        else
        {
            kv_cmd.res->succ = false;
            kv_cmd.res->key = kv_cmd.key;
            kv_cmd.res->value = "";
        }
        break;
    case kv_command::CMD_PUT:
        if (store.count(kv_cmd.key))
        {
            kv_cmd.res->succ = false;
            kv_cmd.res->key = kv_cmd.key;
            kv_cmd.res->value = store[kv_cmd.key];

            store[kv_cmd.key] = (kv_cmd.value);
        }
        else
        {
            kv_cmd.res->succ = true;
            kv_cmd.res->key = kv_cmd.key;
            kv_cmd.res->value = kv_cmd.value;
            store.insert({kv_cmd.key, kv_cmd.value});
        }
        break;
    }
    kv_cmd.res->done = true;
    kv_cmd.res->cv.notify_all();
    mtx.unlock();
    return;
}
