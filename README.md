# How to run the setup

## Step 0 
stay in the root directory of the project and run:

```bash
make clean && make all
```

## For NS
1. Navigate to the NS directory.
2. Run `make all` to build the NS binaries.
3. Start the NS server by running `make run`.
4. Ensure the server is running by checking the logs.

Tl;dr run:
```bash
cd ns
make run
```

# For SS

:::danger
    Make sure to change the ss_ip var in int main() of ss/main.c to the actual ip of the machine you are running the ss on. --- THIS IS IMPORTANT FOR THE NS TO BE ABLE TO COMMUNICATE WITH THE SS. 
:::


1. Navigate to the SS directory.
2. Run `make all` to build the SS binaries.
3. now to start the ss run:

```bash
./bin/ss_server <ip_of_ns> 9091 9001 9002(for client connection) ./storage/<ip_of_ss>
```

# For Client
1. Navigate to the Client directory.
2. Run command:

```bash
./client_app <ip_of_ns> 9090 <username>
```


:::danger
    MAKE SURE TO TURN OFF YOUR FIREWALL @ANUSHKA
:::



## Assumptions

1. *REVERT and CHECKPOINT needs write access and LISTCHECKPOINTS and VIEWCHECKPOINTS need read access
2. *cant create checkpoint without writing anything first
3. Last modified gives when the file was last “written”. Last accessed changes each time u find that file, so even in info the last accessed time will change. Created at returns when the file was made by the owner.
4. NOVEL : can RESTORE until 5 minutes after delete


## Bonus Features
- Checkpointing
- Hirarchical Storage folder structure
- Novel: Restore until 5 minutes after delete

## Contributers 
- @anushkasharma2005 Anushka Sharma
- @kiran-r-10 Kiran R 
