module mAlu (PC, ReadData1, ReadData2, Instruction, ALUSelA, ALUSelB, ALUOp,
    Zero, ALU_result);
 
parameter pBuswidth  = 8;// dummy width

input[pBuswidth-1:0]   PC;
input[pBuswidth-1:0]   ReadData1;
input[pBuswidth-1:0]   ReadData2;
input[15:0]            Instruction;
input                  ALUSelA;
input[1:0]             ALUSelB;
input[1:0]             ALUOp;
 
output                 Zero;
output[pBuswidth-1:0]  ALU_result;

reg[pBuswidth-1:0]     MuxA;
reg[pBuswidth-1:0]     MuxB;
reg                    Zero;
reg[pBuswidth-1:0]     ALU_result;
 
parameter pZero      = 32'b0;
parameter pPositive  = 16'h0000;
parameter pNegative  = 16'hffff;

reg Temp;

always @(ALUSelA or ReadData1 or PC)
   begin
   if(ALUSelA) begin
      MuxA=ReadData1;
      end
   else begin
      MuxA=PC;
      end
   end
 
always @(ALUSelB or ReadData2 or Instruction)
   begin
   case(ALUSelB[1:0])
      2'b00 : MuxB = ReadData2;
      2'b01 : MuxB = 1;                   // Increment address
      2'b10,
      2'b11 : if(Instruction[15]) begin   // Sign extension
                MuxB = {pNegative,Instruction};
                end
              else begin
                MuxB = {pPositive,Instruction};
                end
    endcase
    end
 
always @(ALUOp or Instruction or MuxA or MuxB)
   begin
      casex({ALUOp[1:0],Instruction[5:0]})
        8'b00xxxxxx : ALU_result = MuxA + MuxB; // Addition
        8'b01xxxxxx : ALU_result = MuxA - MuxB; // Subtraction
        /*R-Type operations */
        8'b10100100 : ALU_result = MuxA & MuxB; // AND
        8'b10100101 : ALU_result = MuxA | MuxB; // OR
        8'b10100110 : ALU_result = MuxA ^ MuxB; // XOR
        8'b10100000 : ALU_result = MuxA + MuxB; // Addition
        8'b10100010 : ALU_result = MuxA - MuxB; // Subtraction
        8'b10000010 : ALU_result = MuxB >> 1;   // Shift right
        8'b10000000 : ALU_result = MuxB << 1;   // Shift left
 
        default: ALU_result = 'bx;
      endcase

      if (ALU_result == pZero) begin
         Zero=1;
         end
      else begin
         Zero=0;
         end
   end
endmodule
