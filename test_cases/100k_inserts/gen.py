n_lines = 100000

with open("./edit_script.txt", "w") as f_edit, open("./in_2.txt", "w") as f_in2:

    f_in2.write("1\n2\n3\n")

    for i in range(n_lines):
        f_in2.write("30\n")
        t = (str(i+3),"+ 30\n")
        f_edit.write(" ".join(t))
        
        if(i%10000 == 0):
            print ("\r{:.0f}".format((i/n_lines)*100))
