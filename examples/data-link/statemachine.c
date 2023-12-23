while (STOP == FALSE)
    {
        // Returns after 5 chars have been input
        int bytes = read(fd, read_buf, 1);
        switch (state)
        {
        case 0:
            if(read_buf[0] == FLAG) state = 1;
            else state = 0;
            break;
        case 1:
            if(read_buf[0] == FLAG) state = 1;
            else if(read_buf[0] == SET_A) state = 2;
            else state = 0;
            break;
        case 2:
            if(read_buf[0] == FLAG) state = 1;
            else if(read_buf[0] == SET_C) state = 3;
            else state = 0;
            break;
        case 3:
            if(read_buf[0] == FLAG) state = 1;
            else if(read_buf[0] == SET_A^SET_C) state = 4;
            else state = 0;
            break;
        case 4:
            if(read_buf[0] == FLAG) STOP = TRUE;
            else state = 0;
            break;
        default:
            break;
        }

        printf("var = 0x%02X\n", read_buf[0]);
    }
