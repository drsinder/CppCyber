/**---------------------------------------------------------------------------
** Copyright(c) 2017, Dale Sinder
**
** Name: Program.cd
**
** Description:
** Mail a TUTOR/CODE file print from Cybis.
** Email address (something with an @ in it) should be placed near the
** top of block 1-b.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License version 3 as
**  published by the Free Software Foundation.
**
**  This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
** GNU General Public License version 3 for more details.
**
**  You should have received a copy of the GNU General Public License
**  version 3 along with this program in file "license-gpl-3.0.txt".
**  If not, see<http://www.gnu.org/licenses/gpl-3.0.txt>.
**
**--------------------------------------------------------------------------
*/

using System;
using System.Text;
using System.IO;
using System.Threading.Tasks;
using System.Threading;
using System.Configuration;
using SendGrid;
using SendGrid.Helpers.Mail;

namespace Mail2
{
    class Program
    {
        static void Main(string[] args)
        {
            Thread.Sleep(1000); // wait a bit for Cybis to settle

            if (args.Length == 2)
            {
                Console.Out.WriteLine("\nMailing:  " + args[0]);

                MailAsync(args[0], args[1]).Wait();
            }
            else
                Console.Out.WriteLine("Wrong number of arguments.  Needs file to print and apppath.");

            Console.Write("\nOperator> ");


        }

        static async Task MailAsync(string fileName, string appString)
        {
            StreamReader sr = new StreamReader(fileName);
            string line;
            char obj = '\\';
            int linenum = 0;
            bool gotmailto = false;
            bool gotmailtoname = false;
            string mailto = "";

            Console.WriteLine("Mailing:  " + fileName);
            string fullpath = appString.Substring(0, appString.LastIndexOf(obj) + 1) + fileName;

            Console.WriteLine(fullpath);

            while (!sr.EndOfStream)
            {
                if (linenum++ > 130)    // look up to 130 lines into the file
                    break;
                line = await sr.ReadLineAsync();
                //Console.WriteLine(line);

                if (gotmailto)
                {
                    if (line.Contains("@"))
                    {
                        mailto = line.Substring(9);
                        gotmailtoname = true;
                        break;
                    }
                }
                else
                    if (line.StartsWith("     -------------------------------------------------------------------- part= 1, block=b --------------------------- "))
                    gotmailto = true;

            }
            sr.Close();

            if (!gotmailtoname)
                return;

            Console.WriteLine(mailto);

            StringBuilder sb = new StringBuilder();
            sr = new StreamReader(fileName);
            while (!sr.EndOfStream)
            {
                line = await sr.ReadLineAsync();
                sb.AppendLine(line);
            }

            string message = sb.ToString();

            var plainTextBytes = Encoding.UTF8.GetBytes(message);
            string encodedText = Convert.ToBase64String(plainTextBytes);

            await AuthMessageSender.SendEmailAsync(mailto, "PLATO print", "See attached file.", fileName, encodedText);
        }

    }

    public static class AuthMessageSender
    {
        public static async Task SendEmailAsync(string email, string subject, string message, string fname, string content)
        {
            // Plug in your email service here to send an email.

            AppSettingsReader r = new AppSettingsReader();
            string mykey = (string)r.GetValue("ApiKey", typeof(string));
            string myemail = (string)r.GetValue("Email", typeof(string));
            string myname = (string)r.GetValue("Name", typeof(string));


            var apiKey = mykey;
            var client = new SendGridClient(apiKey);
            var from = new EmailAddress(myemail, myname);
            var to = new EmailAddress(email);
            SendGridMessage msg = MailHelper.CreateSingleEmail(from, to, subject, message, null);

            string fileName = fname.Replace("\\", "_");

            msg.AddAttachment(fileName, content);

            await client.SendEmailAsync(msg);
        }

    }
}
