#!/usr/bin/env python3
"""Script for Tkinter GUI game client."""
# from socket import AF_INET, socket, SOCK_STREAM
import socket
from threading import Thread
import tkinter
import enchant

copyright = "Developed By - Aamil Farooq"
dic = enchant.Dict("en_US")
online_count = 0
name = ""
last_word = ""
is_enable = True


def get_online_count():
    return online_count


def set_online_count(count):
    global online_count
    online_count = count


def set_score():
    score = int(lblscore.cget('text')) + 1
    lblscore.config(text=str(score))


def show_error(error):
    print(error)


def is_valid(word):
    word.split()
    print("inside is_valid")
    err_english = word + " is not an english word"
    err_empty = "Input a word"
    err_match = "Not a valid word"

    if not word:
        show_error(err_empty)
        msg_list.insert(tkinter.END, err_empty)
        msg_list.itemconfig(tkinter.END, fg="red")
        msg_list.yview(tkinter.END)
        return False

    if not dic.check(word):
        show_error(err_english)
        msg_list.insert(tkinter.END, err_english)
        msg_list.itemconfig(tkinter.END, foreground="red")
        msg_list.yview(tkinter.END)
        return False

    if last_word and word[0] != last_word[-1]:
        show_error(err_match)
        msg_list.insert(tkinter.END, err_match)
        msg_list.itemconfig(tkinter.END, fg="red")
        msg_list.yview(tkinter.END)
        return False

    return True


def enable():
    txtbox.config(state=tkinter.NORMAL)
    btnsend.config(state=tkinter.NORMAL)
    btnpass.config(state=tkinter.NORMAL)


def disable():
    txtbox.config(state=tkinter.DISABLED)
    btnsend.config(state=tkinter.DISABLED)
    btnpass.config(state=tkinter.DISABLED)


def set_state(state):
    global is_enable
    if state == tkinter.DISABLED:
        disable()
        is_enable = False
    elif state == tkinter.NORMAL:
        enable()
        is_enable = True


def receive():
    """Handles receiving of messages."""
    global name
    global last_word

    while True:
        try:
            str_recd = client_socket.recv(BUFSIZ).decode('utf-8')
            print("str_recd: "+str_recd)
            code = str_recd.split('-')[0]
            st = str_recd.split('-')[1]

            if code == "UNAME_OK":
                s1 = "Welcome " + name + " !"
                s2 = "Enter english word starting with the last letter of previous word."
                lblname.config(text=name)
                msg_list.insert(tkinter.END, s1)
                msg_list.itemconfig(tkinter.END, fg="green")
                msg_list.insert(tkinter.END, s2)
                msg_list.itemconfig(tkinter.END, fg="blue")
                msg_list.yview(tkinter.END)
                my_msg.set("")
                count = str_recd.split('-')[2]
                set_online_count(count)

            elif code == "UNAME_FAIL":
                msg = "The username \"" + name + "\" is taken. Try another name."
                name = ""
                print(msg)
                msg_list.insert(tkinter.END, msg)
                msg_list.itemconfig(tkinter.END, fg="red")
                msg_list.yview(tkinter.END)
            elif code == "PASS_OK":
                disable()
            elif code == "WORD_OK":
                set_score()
                msg_list.insert(tkinter.END, name+" : "+my_msg.get())
                msg_list.itemconfig(tkinter.END, fg="magenta")
                last_word = my_msg.get()
                last_word.strip()
                disable()
                # msg_list.insert(tkinter.END, name + " : " + my_msg.get())
            elif code == "WORD_EXIST":
                error = my_msg.get() + " already entered. Input a new word."
                msg_list.insert(tkinter.END, error)
                msg_list.itemconfig(tkinter.END, fg="red")
            elif code == "WELCOME":
                greet = "Greetings! Type your name and press enter!"
                txtbox.focus_set()
                msg_list.insert(tkinter.END, greet)
                msg_list.itemconfig(tkinter.END, fg="blue")
            # elif code == "ENABLE":
            #    enable()
            elif code == "BROADCAST":   # msg like join, quit, pass, new word
                count = str_recd.split('-')[2]
                set_online_count(count)
                msg = str_recd.split('-')[3]
                if ":" in msg:
                    last_word = msg.split(':')[1]
                    last_word.strip()
                    print("last word: "+last_word)
                msg_list.insert(tkinter.END, msg)
                msg_list.itemconfig(tkinter.END, fg="magenta")
                print(msg + ": " + my_msg.get())

            msg_list.yview(tkinter.END)
            my_msg.set("")
            lblonline.config(text=str(get_online_count())+" Live")

            set_state(st)
        except OSError:  # Possibly client has left the game.
            break


def send(event=None):  # event is passed by binders.
    """Handles sending of messages."""
    if not is_enable:
        return

    global name
    word = my_msg.get()
    # my_msg.set("")  # Clears input field.

    if name == "":
        name = word
        str_to_send = "UNAME-"+name
        print("str_to_send: "+str_to_send)
        client_socket.send(str.encode(str(str_to_send)))
    else:
        if is_valid(word):
            str_to_send = "WORD-" + word
            print("str_to_send: " + str_to_send)
            client_socket.send(str.encode(str(str_to_send)))
        else:
            pass


def _quit():  # event is passed by binders.
    """Handles sending of messages."""
    str_to_send = "QUIT-"
    print("str_to_send: " + str_to_send)
    client_socket.send(str.encode(str(str_to_send)))
    client_socket.close()
    top.quit()


def _pass():  # event is passed by binders.
    """Handles sending of messages."""
    str_to_send = "PASS-"
    print("str_to_send: " + str_to_send)
    client_socket.send(str.encode(str(str_to_send)))


def on_closing(event=None):
    """This function is to be called when the window is closed."""
    _quit()


top = tkinter.Tk()
w = 450
h = 350

ws = top.winfo_screenwidth()
hs = top.winfo_screenheight()

x = (ws / 2) - w / 2
y = (hs / 2) - h / 2

top.geometry("%dx%d+%d+%d" % (w, h, x, y))

top.title("Word Chain Game v2.2")

row = tkinter.Frame(top)
lblname = tkinter.Label(row, text='', fg='blue', width=40)
lblonline = tkinter.Label(row, text='', fg='blue', width=40)
row.pack(pady=5)
lblname.pack(side=tkinter.LEFT)
lblonline.pack(side=tkinter.RIGHT)

messages_frame = tkinter.Frame(top)
my_msg = tkinter.StringVar()  # For the messages to be sent.
my_msg.set("")
scrollbar = tkinter.Scrollbar(messages_frame)  # To navigate through past messages.
# Following will contain the messages.
msg_list = tkinter.Listbox(messages_frame, height=12, width=45, yscrollcommand=scrollbar.set)
scrollbar.pack(side=tkinter.RIGHT, fill=tkinter.Y)
msg_list.pack(side=tkinter.LEFT, fill=tkinter.BOTH)
msg_list.pack()
messages_frame.pack(pady=5)

row = tkinter.Frame(top)
txtbox = tkinter.Entry(row, textvariable=my_msg, width=15, bg='yellow')
txtbox.bind("<Return>", send)
btnsend = tkinter.Button(row, text="OK", width=5, command=send)
btnpass = tkinter.Button(row, text="Pass", width=5, command=_pass)
lblscore = tkinter.Label(row, text='0', width=3)
# btnsend.pack(side=tkinter.LEFT)
row.pack(pady=5)
txtbox.pack(side=tkinter.LEFT, padx=5)
lblscore.pack(side=tkinter.RIGHT, padx=5)
btnpass.pack(side=tkinter.RIGHT, padx=5)
btnsend.pack(side=tkinter.RIGHT, padx=5)
# btnsend.place(relx=0.8, rely=0.8, anchor=tkinter.CENTER)

row = tkinter.Frame(top)
btnquit = tkinter.Button(row, text="Quit", width=5, command=_quit)
row.pack(pady=5)
btnquit.pack(side=tkinter.RIGHT)

row = tkinter.Frame(top)
cpy_label = tkinter.Label(row, text=copyright, fg='blue', width=40, justify=tkinter.CENTER)
row.pack(pady=5)
cpy_label.pack()

top.protocol("WM_DELETE_WINDOW", on_closing)


# ----Now comes the sockets part----
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# HOST = input('Enter host: ')
HOST = '172.22.170.104'
#HOST = '127.0.0.1'
#HOST = socket.gethostbyname('BANL1648dc7d3.local')
#HOST = socket.gethostname()
#print(HOST)
#print(socket.gethostbyname(HOST))
#PORT = input('Enter port: ')
#if not PORT:
#    PORT = 34000
#else:
#    PORT = int(PORT)
PORT = 34000

BUFSIZ = 1024
ADDR = (HOST, PORT)

client_socket.connect(ADDR)

receive_thread = Thread(target=receive)
receive_thread.start()
tkinter.mainloop()  # Starts GUI execution.