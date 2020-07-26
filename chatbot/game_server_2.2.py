#!/usr/bin/env python3
"""Server for multithreaded (asynchronous) game application."""
from socket import AF_INET, socket, SOCK_STREAM
from threading import Thread
#import enchant
import tkinter
import time


wordlist = []
namelist = []
clientsocklist = []
clientobjlist = []
addresses = {}
#dic = enchant.Dict("en_US")


class Clients:
    def __init__(self, name, sockid):
        self.name = name
        self.sockid = sockid
        self.words_sent = 0
        self.b_enable = False

    def is_ready(self):
        return self.words_sent > 0

    def add_words_count(self):
        self.words_sent += 1

    def get_name(self):
        return self.name

    def get_sockid(self):
        return self.sockid

    def is_enabled(self):
        return self.b_enable

    def set_enabled(self, val):
        self.b_enable = val


def add_client(name, sockid):
    clientobjlist.append(Clients(name, sockid))
    namelist.append(name)


def remove_client(name):
    # clientobjlist.remove()
    pass


def is_new(word):
    if word not in wordlist:
        wordlist.append(word)
        return True
    return False


def get_client_count():
    return len(namelist)


def close_thread(client):
    print("Thread closing for " + str(client))
    client.close()
    # clientsocklist.remove(client)


def get_next(curname):
    print("======inside get_next")
    print("curname: "+curname)
    index = namelist.index(curname)
    print("cur index: %d" % index)

    if index != len(namelist) - 1:
        index = index + 1
    else:
        index = 0

    print("next index: %d" % index)
    print("======leaving get_next")
    return index


"""
def enable_next(name):
    if get_client_count() < 2:
        return

    # looking into obj list
    for cli in clientobjlist:
        if cli.get_name() == name:
            index = get_next(name)
            next_client = clientobjlist[index]
            str_to_send = "ENABLE-"
            print("str_to_send: " + str_to_send)
            next_client.get_sockid().send(str.encode(str_to_send))
            break
"""


def quit_client(name, sockid):
    print("******inside quit_client")
    print("client count: %s" % len(clientobjlist))
    # looking into obj list
    index_to_enable = -1
    for cli in clientobjlist:
        print("quitting client name: "+cli.get_name())
        if cli.get_name() == name:
            print("quitting client present")
            if cli.is_enabled():
                print("quitting client is enabled")
                index_to_enable = get_next(name)
                print("index_to_enable: %d" % index_to_enable)

            sockid.close()

            msg = name + " has quit the game !"
            print(msg)
            broadcast(msg, sockid, get_client_count()-1, index_to_enable)

            clientobjlist.remove(cli)
            namelist.remove(name)

            break

    print("******leaving quit_client")


def accept_incoming_connections():
    """Sets up handling for incoming clients."""
    while True:

        client_sockid, client_address = SERVER.accept()

        if get_client_count() == 0:
            wordlist.clear()

        print("%s:%s has connected." % client_address)
        # welcome = "WELCOME"
        str_to_send = "WELCOME-"
        print("msg_to_send: "+str_to_send)
        client_sockid.send(str.encode(str_to_send))

        Thread(target=handle_client, args=(client_sockid,)).start()


def handle_client(client):  # Takes client socket as argument.
    """Thread - Handles a single client connection."""

    # join
    name = ""
    while True:
        print("Waiting for name")
        str_recd = client.recv(BUFSIZ).decode("utf8")
        print("str_recd: "+str_recd)
        code = str_recd.split('-')[0]
        name = str_recd.split('-')[1]

        print("Received " + name)
        if code == "QUIT":
            close_thread(client)
            return
        elif code == "UNAME":
            if len(namelist) > 0 and name in namelist:  # checking duplicate username
                # msg = "The username \"" + name + "\" is taken. Try another name."
                str_to_send = "UNAME_FAIL-"
                print("str_to_send: " + str_to_send)
                client.send(str.encode(str_to_send))
            else:
                add_client(name, client)  # create new client and add to clientobjlist

                st = ""
                # a new client does not know words history, so disable initially
                if get_client_count() > 1:
                    st = tkinter.DISABLED

                str_to_send = "UNAME_OK-" + st + "-" + str(get_client_count())
                print("str_to_send: " + str_to_send)
                client.send(str.encode(str_to_send))
                break

    msg = "%s has joined the game !" % name
    # print(msg)
    broadcast(msg, client, get_client_count())

    # play
    while True:
        try:
            print("Thread waiting for word from "+name)
            str_recd = client.recv(BUFSIZ).decode('utf-8')
            print("str_recd: " + str_recd)
            code = str_recd.split('-')[0]
            word = str_recd.split('-')[1]

            if code == "QUIT":  # client closed
                quit_client(name, client)
                break
            elif code == "PASS":
                curindex = namelist.index(name)
                next_index = get_next(name)

                while not clientobjlist[next_index].is_ready():
                    next_index = get_next(clientobjlist[next_index].get_name())

                if curindex != next_index:
                    str_to_send = "PASS_OK-"
                    print("str_to_send: " + str_to_send)
                    client.send(str.encode(str_to_send))
                    msg = name + " has given a pass !"
                    time.sleep(0.2)

                broadcast(word, client, get_client_count(), next_index, msg)
            elif code == "WORD":
                if is_new(str(word)):  # new word
                    next_index = get_next(name)
                    str_to_send = "WORD_OK-"+"-"+word
                    print("str_to_send: " + str_to_send)
                    client.send(str.encode(str_to_send))
                    time.sleep(0.2)
                    broadcast_word(word, client, get_client_count(), next_index, name + " : ")
                else:
                    str_to_send = "WORD_EXIST-"
                    print("str_to_send: " + str_to_send)
                    client.send(str.encode(str_to_send))

                print(name + ": " + word)
                print(wordlist)

        except OSError:
            print("Exception caught")
            break


def broadcast_word(msg, cur_sockid, live_count, index_to_enable=-1, prefix=""):  # prefix is for name identification.
    """Broadcasts a message to all the clients."""

    print("******inside broadcast_word")
    print(msg)
    print("clients count: %d" % live_count)

    for cli in clientobjlist:
        sockid = cli.get_sockid()
        cur_index = clientobjlist.index(cli)

        print(sockid)
        print("name: "+cli.get_name())
        # print("next index: %d" % next_client)

        if index_to_enable < 0 or sockid == cur_sockid:  # join or quit msg broadcast
            # if sockid == cur_sockid:
            continue
            # st = ""
        elif cur_index == index_to_enable:
            st = tkinter.NORMAL
            cli.set_enabled(True)
            cli.add_words_count()
        else:
            st = tkinter.DISABLED
            cli.set_enabled(False)
            cli.add_words_count()

        str_to_send = "BROADCAST-" + st + "-" + str(live_count) + "-" + str(prefix + msg)
        print("str_to_send: " + str_to_send)
        sockid.send(str.encode(str_to_send))

    print("******leaving broadcast_word")


def broadcast(msg, cur_sockid, live_count, index_to_enable=-1, prefix=""):  # prefix is for name identification.
    """Broadcasts a message to all the clients."""

    print("******inside broadcast")
    print(msg)
    print("clients count: %d" % live_count)

    for cli in clientobjlist:
        sockid = cli.get_sockid()
        cur_index = clientobjlist.index(cli)
        st = ""

        print(sockid)
        print("name: "+cli.get_name())
        # print("next index: %d" % next_client)

        if sockid == cur_sockid:  # join msg broadcast
            # if sockid == cur_sockid:
            continue

        if index_to_enable >= 0:
            if cur_index == index_to_enable:
                st = tkinter.NORMAL
                cli.set_enabled(True)
            else:
                st = tkinter.DISABLED
                cli.set_enabled(False)

        str_to_send = "BROADCAST-" + st + "-" + str(live_count) + "-" + str(prefix + msg)
        print("str_to_send: " + str_to_send)
        sockid.send(str.encode(str_to_send))

    print("******leaving broadcast")


HOST = ''
PORT = 34000
BUFSIZ = 1024
ADDR = (HOST, PORT)

SERVER = socket(AF_INET, SOCK_STREAM)
SERVER.bind(ADDR)

if __name__ == "__main__":
    SERVER.listen(0)
    print("Waiting for connection...")
    try:
        ACCEPT_THREAD = Thread(target=accept_incoming_connections)
        ACCEPT_THREAD.start()
        ACCEPT_THREAD.join()
        SERVER.close()
    except OSError:
        print("Exception caught")
        pass
