(function() {
	window.Main = {};
	Main.Page = (function() {
		var mosq = null;
		function Page() {
			var _this = this;
			mosq = new Mosquitto();

			$('#connect-button').click(function() {
				return _this.connect();
			});
			$('#disconnect-button').click(function() {
				return _this.disconnect();
			});
			$('#subscribe-button').click(function() {
				return _this.subscribe();
			});
			$('#unsubscribe-button').click(function() {
				return _this.unsubscribe();
			});
			
			
			$('#botao-pesagem').click(function() {
				var payload = "P";  
				var TopicPublish = "MQTTBalancaRecebe";				
				mosq.publish(TopicPublish, payload, 0);
			});
			
			
			$('#botao-preco-por-kg').click(function() {
				var TopicPublish = "MQTTBalancaRecebe";
				var PrecoPorKG = document.getElementById('preco-por-kg').value	
				var PayloadPrecoPorKG = "V"+PrecoPorKG;

				//testa se o valor/kg não está vazio
				if (PrecoPorKG.length == 0) {
					alert("Erro: nenhum valor de preço/kg digitado!");
					return;
				}
				
				//testa se o conteudo digitado é um número
				if (isNaN(PrecoPorKG)){
					alert("Erro: valor de preço/kg digitado nao é um numero!");
					return;
				}
																
				mosq.publish(TopicPublish, PayloadPrecoPorKG, 0);
			});
                        
                        
                        $('#botao-tempo-funcionamento-demonstracao').click(function() {
				var TopicPublish = "MQTTBalancaRecebe";
				var TempoFunc = document.getElementById('tempo_fucionamento_demonstracao').value	
				var PayloadTempoFunc = "T"+TempoFunc;

				//testa se o valor/kg não está vazio
				if (TempoFunc.length == 0) {
					alert("Erro: nenhum valor de tempo digitado!");
					return;
				}
				
				//testa se o conteudo digitado é um número
				if (isNaN(TempoFunc)){
					alert("Erro: valor de tempo digitado nao é um numero!");
					return;
				}
																
				mosq.publish(TopicPublish, PayloadTempoFunc, 0);
			});
			
			mosq.onconnect = function(rc){
				var p = document.createElement("p");
				var topic = "MQTTBalancaEnvia"
				p.innerHTML = "<font color='green'>Conectado ao Broker!</font>";
				$("#debug").html(p);
				mosq.subscribe(topic, 0);
				
			};
			mosq.ondisconnect = function(rc){
				var p = document.createElement("p");
				var url = "ws://iot.eclipse.org/ws";
				
				p.innerHTML = "<font color='red'>A conexão com o broker foi perdida.</font>";
				$("#debug").html(p);				
			};
			
			mosq.onmessage = function(topic, payload, qos){
				var p = document.createElement("p");
				var PayloadParseado = payload.split("-");
				var PesoBalanca = PayloadParseado[0];
				var ValorTotalPesagem = PayloadParseado[1];
				var ValorPorKG = PayloadParseado[2];
				var FlagPlataformaZerada = PayloadParseado[3];
				var FlagPlataformaEstavel = PayloadParseado[4];
				var FlagBalancaCalibrada = PayloadParseado[5];
                                var StatusManutencaoPreventiva = PayloadParseado[6];
				
				//compoe o inner html com os dados recebidos
				p.innerHTML = "<table border='0' width='70%'><tr><td><center><b><font color='blue' size ='6'>Peso: "+PesoBalanca+"g <br>Valor / kg: R$"+ValorPorKG+" <br>Valor total: R$"+ValorTotalPesagem+"</font><br></b><br></center></td> <td><center> Plataforma zerada:    <img src='"+FlagPlataformaZerada+".png' height='20' width='20'><br>Plataforma estável:   <img src='"+FlagPlataformaEstavel+".png' height='20' width='20'><br>Plataforma calibrada: <img src='"+FlagBalancaCalibrada+".png' height='20' width='20'><br></center></td><td><table border=0><tr><td>Manutenção preventiva:</td><td><img src='"+StatusManutencaoPreventiva+".png' height='80' width='80'></td></tr></table></td></tr></table>";
                			
				
				$("#status_io").html(p);
			};
		}
		Page.prototype.connect = function(){
			var url = "ws://iot.eclipse.org/ws";
			mosq.connect(url);
		};
		Page.prototype.disconnect = function(){
			mosq.disconnect();
		};
		Page.prototype.subscribe = function(){
			var topic = "MQTTBalancaEnvia";
			mosq.subscribe(topic, 0);
		};
		Page.prototype.unsubscribe = function(){
			var topic = "MQTTBalancaEnvia";;
			mosq.unsubscribe(topic);
		};
		
		return Page;
	})();
	$(function(){
		return Main.controller = new Main.Page;
	});
}).call(this);

