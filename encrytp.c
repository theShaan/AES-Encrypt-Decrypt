#include <stdio.h>
#include <stdlib.h>
#include <gcrypt.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define bool int
#define true 1
#define false 0
#define SALT "NaCl"
#define ITERATIONS 4096
#define KDF_KEY_SIZE 16

int encrypt();
char *out_file_name;

int main(int argc, char *argv[])
{

	if (argc != 3 && argc != 4)
	{
		printf("Invalid number of parameters\n");
		return -1;
	}
	else
	{
		if (strcmp(argv[2], "-l") == 0)
		{
			return encrypt(argv, false);
		}
		else if (strcmp(argv[2], "-d") == 0)
		{
           return encrypt(argv, true);
		}
		else if (strcmp(argv[2], "-h") == 0)
		{
			FILE *input_fp=fopen(argv[1], "r"); 
			fseek(input_fp, 0, SEEK_END);
			int file_size = ftell(input_fp);
            fseek(input_fp, 0, SEEK_SET);
            char *input_buffer = (char *) malloc (file_size);
			char *digest = malloc(sizeof(gcry_md_get_algo_dlen(GCRY_MD_SHA512)));
			
			fread(input_buffer, sizeof(char), file_size, input_fp);
			gcry_md_hash_buffer(GCRY_MD_SHA512,digest,input_buffer,file_size);
			
			printf("Hash of the file is : \n");
			
			for(int i = 0; i < strlen(digest); i++)
		    printf("%02X ",(unsigned char) digest[i]);
		    printf("\n");
		}
		
		else
		{
			printf("Invalid parameters\n");
			return -1;
		}
		
	}
return 0;
}

int encrypt(char *argv[], bool transfer_needed)
{
	out_file_name = (char *)malloc(strlen(argv[1])+3);
	strcat(out_file_name,argv[1] );
	strcat(out_file_name,".uf" );
	char pass[100], key[KDF_KEY_SIZE];
	char *file_buffer, *ecrypted_file_buffer;
	int len = 0;
	int sucess = 0;
	FILE *input_fp, *output_fp, *output_fp2;
			gcry_error_t err;
		gcry_md_hd_t md;
			char *hmac;
	int IV[KDF_KEY_SIZE] = {5844};
	
	
	printf("Beginning encryption\n");
	printf("Please enter password\n");
	scanf("%s", &pass);

	gcry_kdf_derive(pass,
                        strlen(pass),
                        GCRY_KDF_PBKDF2,
                        GCRY_MD_SHA512,
                        SALT,
                        strlen(SALT),
                        ITERATIONS,
                        KDF_KEY_SIZE,
                        key);
			
    if (strlen(key) == 0)
	{
		printf("Error: Generating key, exiting...\n");
		return -1;
	}	

	//printf("Key: ");
	for(int i = 0; i < KDF_KEY_SIZE; i++)
		printf("%02X ",(unsigned char) key[i]);
	printf("\n");

    //File operations
	input_fp=fopen(argv[1], "r"); 
	if (!input_fp) {
  		printf("Error: Opening file, exiting...\n");
  		return -1;
	}
	
	//Get file size
	fseek(input_fp, 0, SEEK_END);
	int file_size = ftell(input_fp);
        fseek(input_fp, 0, SEEK_END);
	file_size = ftell(input_fp);
        fseek(input_fp, 0, SEEK_SET);
        int mod = file_size % KDF_KEY_SIZE;
        int pad = KDF_KEY_SIZE - mod;

            
         
	file_buffer = (char *) malloc ((file_size + pad) * sizeof(char));
	ecrypted_file_buffer = (char *) malloc ((file_size + pad) * sizeof(char));
	fseek(input_fp, 0, SEEK_SET);
	fread(file_buffer, sizeof(char), file_size, input_fp);
        
        //Fix size
        
        //int mod = file_size % KDF_KEY_SIZE;
        if (mod != 0)
        {
            char temp = file_buffer[file_size];
            for (int i=0; i < KDF_KEY_SIZE - mod; i++)
            {
                file_buffer[file_size++] = '\0';
            }
            file_buffer[file_size] = temp;
            
        }
	//Begin encrytion
	gcry_cipher_hd_t g_cipher_handle;
	gcry_error_t g_err;


	g_err = gcry_cipher_open(&g_cipher_handle, GCRY_CIPHER_AES128 , GCRY_CIPHER_MODE_CBC , GCRY_CIPHER_SECURE);
    g_err = gcry_cipher_setkey(g_cipher_handle, key, KDF_KEY_SIZE);	
    g_err = gcry_cipher_setiv(g_cipher_handle, &IV, KDF_KEY_SIZE);

    //g_err = gcry_cipher_encrypt(g_cipher_handle, ecrypted_file_buffer, file_size, file_buffer, file_size);
	g_err = gcry_cipher_encrypt(g_cipher_handle, file_buffer, file_size, NULL, 0);

    if(g_err != 0)
	{
		printf ("Error: During encryption %s\n",gcry_strerror(g_err));
		return -1;
	}

	//Generate the hmac

	{


		err = gcry_md_open(&md, GCRY_MD_SHA512, GCRY_MD_FLAG_HMAC | GCRY_MD_FLAG_SECURE);
		err = gcry_md_enable(md,GCRY_MD_SHA512);
		err = gcry_md_setkey(md, key,KDF_KEY_SIZE);
		
		gcry_md_write(md,file_buffer,file_size);
		gcry_md_final(md);
		hmac = gcry_md_read(md , GCRY_MD_SHA512 );

	}
	
	//Save the file
	{		

	if( access( out_file_name, R_OK ) != -1 ) 
	{
	   	printf ("File already present\n");
	    return 33;
	} 
		output_fp = fopen(out_file_name,"w");
		fwrite(file_buffer, file_size, sizeof(char), output_fp);
		fwrite(hmac, 64 , sizeof(char), output_fp);		
		fclose(input_fp);
		fclose(output_fp);
	}
	
	printf("Successfully encrypted. Encrypted file is %s\n", out_file_name);

	//Transfer if needed
	if (transfer_needed)
	{
		output_fp = fopen(out_file_name,"r");
		int sockfd = socket(AF_INET, SOCK_STREAM, 0); // socket handler 
		struct sockaddr_in dest_sock_addr; // server address 
		char *ip = strtok(argv[3],":");
	    int port = atoi(strtok(NULL, ":")), bytes_read = 0; 

		// Open the socket handler to use it to connect to server
		if(sockfd < 0)
		{	
			printf("Error : Could not create socket \n");
				fclose(output_fp);
			return -1;
		}

		dest_sock_addr.sin_family = AF_INET;
		dest_sock_addr.sin_port = htons(port); 
		dest_sock_addr.sin_addr.s_addr = inet_addr(ip); 

		if(connect(sockfd, (struct sockaddr *)&dest_sock_addr, sizeof(dest_sock_addr))<0)
			{
				printf("\n Error in establishing connection to Server\n");
					fclose(output_fp);
				return -1;				
			}

		printf("Transmitting to %s:%d\n",ip,port);

		while(1)
		{
        unsigned char buff[1024]={0};
        bytes_read = fread(buff,1,1024,output_fp);
        write(sockfd, buff, bytes_read);
            if (bytes_read != 1024)
            break;
        }
		
    printf("Successfully sent the file\n");
    fclose(output_fp);
    }
return 0;	

	}

